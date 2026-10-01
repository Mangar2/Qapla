#!/bin/bash
# Brings up a spot instance and lets it carry on where the last one stopped. Nothing here is done
# by hand, because everything here went wrong by hand at least once on 28.09.2026.
#
#   sh src/pipeline/aws-resume.sh <instance type> <host key> [<s3 prefix to finish> <object count>]
#
# The host key is the hosts entry in pipeline.toml whose steps this instance is to run. Every
# instance gets its own - aws, aws5, aws6 - so two of them never reach for the same step.
#
# Choosing the instance type is the one decision left outside - it depends on what the work is and
# on what the spot quota allows. Everything else follows from it:
#
#   * the availability zones are tried cheapest first, because the spread between them reached a
#     factor of 2.8 in one measurement - and the cheapest quotes are regularly the ones with no
#     capacity, so a refusal moves on to the next zone instead of ending the run
#   * the bid is set above the zone's own price, since a request below the zone's floor is refused
#     outright with SpotMaxPriceTooLow
#   * the instance is brought to the state a helper needs by bootstrap-instance.sh, which refuses to
#     finish rather than leave a machine that only looks ready
#   * the pipeline is started detached, so the machine that launched it may sleep or be switched off
#   * a watchdog ends the instance when its work has arrived in s3, and only then
#
# The pipeline picks up by itself: a chunk whose result is already in s3 is skipped, so a replacement
# loses at most the chunk the reclaimed instance had in flight.
set -e
TYPE=${1:?usage: aws-resume.sh <instance type> <host key> [<s3 prefix> <object count>]}
HOST=${2:?which hosts entry of pipeline.toml this instance serves}
WATCH_PREFIX=${3:-}
WATCH_COUNT=${4:-}

HERE=$(cd "$(dirname "$0")" && pwd)
LOCAL="$HERE/local.toml"
# A spot instance can be taken away in the middle of being set up. Without deadlines an ssh to a
# machine that is already gone waits for as long as the kernel keeps the socket, and the whole
# launch hangs - it did, for twenty minutes, while two thirds of the quota stood idle.
SSH="ssh -o StrictHostKeyChecking=no -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=4"
[ -f "$LOCAL" ] || { echo "$LOCAL is missing - copy local.example.toml and fill it in"; exit 1; }

# The image, the key, the group, the role, the bucket: all of them belong to one installation, so
# they live in local.toml, which is not in the repository. This file names none of them.
value() { python3 -c "import tomllib,sys;print(tomllib.load(open('$LOCAL','rb'))['aws']['$1'])"; }
REGION=$(value region)
# The image and the disk may be overridden: a training machine needs a gpu image with cuda and torch
# already on it, which is a different architecture and a much larger root volume than the arm workers.
IMAGE=${QAPLA_IMAGE:-$(value image)}
KEY=$(value key)
GROUP=$(value group)
PROFILE=$(value profile)
BUCKET=$(value bucket)
VOLUME=${QAPLA_VOLUME:-$(value volume)}
PEM=$(eval echo "$(value pem)")

USERDATA=$(mktemp /tmp/qapla-userdata.XXXXXX)
cat > "$USERDATA" <<'UD'
#!/bin/bash
exec > /var/log/qapla-bootstrap.log 2>&1
set -x
apt-get update -y
apt-get install -y build-essential git make unzip cmake clang python3-venv python3-pip
if ! command -v aws >/dev/null; then
  curl -s "https://awscli.amazonaws.com/awscli-exe-linux-aarch64.zip" -o /tmp/awscli.zip
  unzip -q /tmp/awscli.zip -d /tmp && /tmp/aws/install
fi
touch /home/ubuntu/BOOTSTRAP-DONE && chown ubuntu:ubuntu /home/ubuntu/BOOTSTRAP-DONE
UD

echo "== zones by price for $TYPE =="
ZONES=$(aws ec2 describe-spot-price-history --region $REGION --instance-types "$TYPE" \
        --product-descriptions "Linux/UNIX" --start-time "$(date -u -v-20M +%Y-%m-%dT%H:%M:%S 2>/dev/null || date -u -d '20 minutes ago' +%Y-%m-%dT%H:%M:%S)" \
        --query 'SpotPriceHistory[*].[AvailabilityZone,SpotPrice]' --output text \
        | sort -u -k1,1 | sort -k2 -n)
echo "$ZONES" | sed 's/^/   /'

ID=""
while read -r ZONE PRICE; do
    [ -z "$ZONE" ] && continue
    SUBNET=$(aws ec2 describe-subnets --region $REGION \
             --filters Name=availability-zone,Values="$ZONE" Name=default-for-az,Values=true \
             --query 'Subnets[0].SubnetId' --output text)
    [ "$SUBNET" = "None" ] && continue
    BID=$(python3 -c "print(f'{$PRICE * 1.3:.4f}')")
    echo "== trying $ZONE at $PRICE, bidding $BID =="
    ID=$(aws ec2 run-instances --region $REGION --image-id $IMAGE --instance-type "$TYPE" \
         --key-name $KEY --subnet-id "$SUBNET" --security-group-ids $GROUP \
         --associate-public-ip-address --iam-instance-profile Name=$PROFILE \
         --block-device-mappings "[{\"DeviceName\":\"/dev/sda1\",\"Ebs\":{\"VolumeSize\":$VOLUME,\"VolumeType\":\"gp3\",\"DeleteOnTermination\":true}}]" \
         --instance-market-options "{\"MarketType\":\"spot\",\"SpotOptions\":{\"MaxPrice\":\"$BID\",\"SpotInstanceType\":\"one-time\"}}" \
         --user-data "file://$USERDATA" \
         --tag-specifications "ResourceType=instance,Tags=[{Key=Name,Value=qapla-$HOST}]" \
         --query 'Instances[0].InstanceId' --output text 2>&1 | tail -1)
    case "$ID" in
        i-*) echo "   started $ID in $ZONE at about $PRICE per hour"; break ;;
        *MaxSpotInstanceCountExceeded*)
            # The quota counts the vcpus of every running spot instance in the region together, so
            # no other zone will help - and a terminating instance still holds its share for a
            # while. Say so instead of walking through every zone for the same refusal.
            echo "   the spot vcpu quota of this region is full - another zone will not help."
            echo "   wait for an instance to release its vcpus, or ask for a higher quota."
            rm -f "$USERDATA"; exit 2 ;;
        *)   echo "   refused: $(echo "$ID" | cut -c1-120)"; ID="" ;;
    esac
done <<< "$ZONES"
rm -f "$USERDATA"
[ -n "$ID" ] || { echo "no zone had capacity for $TYPE"; exit 1; }

aws ec2 wait instance-running --region $REGION --instance-ids "$ID"
IP=$(aws ec2 describe-instances --region $REGION --instance-ids "$ID" \
     --query 'Reservations[0].Instances[0].PublicIpAddress' --output text)
echo "== $ID is up at $IP, waiting for its first boot =="
WAITED=0
until $SSH -i "$PEM" ubuntu@"$IP" 'test -f BOOTSTRAP-DONE' 2>/dev/null; do
    sleep 10
    WAITED=$((WAITED + 10))
    if [ $WAITED -ge 300 ]; then
        echo "   it did not come up within five minutes - checking whether it is still there"
        STATE=$(aws ec2 describe-instances --region $REGION --instance-ids "$ID" \
                --query 'Reservations[0].Instances[0].State.Name' --output text 2>/dev/null)
        echo "   $ID is $STATE"
        exit 3
    fi
done

echo "== bringing it to the state a helper needs =="
$SSH -i "$PEM" ubuntu@"$IP" 'bash -s' -- "$BUCKET" < "$HERE/bootstrap-instance.sh"

# local.toml is not in the repository, so it cannot arrive by git pull - it is handed over here.
scp -q -o StrictHostKeyChecking=no -o ConnectTimeout=15 -i "$PEM" "$LOCAL" \
    ubuntu@"$IP":Qapla/src/pipeline/local.toml

# "worker" means the machine takes its work out of the table and is bound to nothing; anything else
# is a host key of the older, machine-bound arrangement. A worker also switches the machine off when
# the table is empty, which is the cost control: no work, no machine.
if [ "$HOST" = train ]; then
    # A training machine runs one thing and nothing else. The files are named here rather than in the
    # script, because which sets are trained together is the experiment.
    echo "== bringing up the training, detached =="
    $SSH -i "$PEM" ubuntu@"$IP" 'bash -s' -- "$BUCKET" "${QAPLA_RUN:?QAPLA_RUN names the training}" \
        ${QAPLA_FILES:?QAPLA_FILES names the game files} < "$HERE/train-instance.sh"
elif [ "$HOST" = worker ]; then
    echo "== starting the worker, detached =="
    $SSH -n -i "$PEM" ubuntu@"$IP" \
        "cd ~/Qapla && ( setsid nohup sh -c 'python3 src/pipeline/worker.py \
         >> test/log/worker-run.log 2>&1; sudo shutdown -h now' < /dev/null & ) ; \
         sleep 8; tail -3 test/log/worker-run.log"
else
    echo "== starting the pipeline, detached =="
    $SSH -n -i "$PEM" ubuntu@"$IP" \
        "cd ~/Qapla && ( setsid nohup python3 src/pipeline/pipeline.py run --host $HOST \
         >> test/log/pipeline-nohup.log 2>&1 < /dev/null & ) ; sleep 8; tail -3 test/log/pipeline.log"
fi

if [ -n "$WATCH_PREFIX" ] && [ -n "$WATCH_COUNT" ]; then
    echo "== arming the watchdog: $WATCH_COUNT objects under $WATCH_PREFIX =="
    ssh -n -o StrictHostKeyChecking=no -i "$PEM" ubuntu@"$IP" \
        "cd ~/Qapla && ( setsid nohup sh src/pipeline/aws-watchdog.sh '$WATCH_PREFIX' '$WATCH_COUNT' \
         >/dev/null 2>&1 & ) ; sleep 2; echo armed"
fi

echo
echo "instance $ID at $IP"
echo "   progress: ssh -i $PEM ubuntu@$IP 'tail -3 ~/Qapla/test/log/pipeline.log'"
