#!/bin/sh
# Creates the task table the workers take their work out of. Run once per installation.
#
#     sh src/pipeline/create-table.sh
#
# The table was made by hand the first time and deleted when the six sets were finished, which is what
# this file is for: the shape of it is not something to rediscover.
#
# One item is one piece of work. "id" is "job#<set>#<kind>#<chunk>" for a single piece and
# "range#<set>#<kind>" for a stretch of them, and it is the only key - a piece is taken by a
# conditional write on that key, so two machines can never take the same one. "status" and "rank"
# carry the index the workers read: the open pieces in the order they should be taken, lowest rank
# first, which is what puts work that was given back ahead of work nobody has started.
#
# Pay per request, because the table is idle for hours and then read by three machines at once, and
# because a few thousand writes a day cost nothing worth reserving capacity for.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
LOCAL="$HERE/local.toml"
[ -f "$LOCAL" ] || { echo "$LOCAL is missing - copy local.example.toml and fill it in"; exit 1; }
value() { python3 -c "import tomllib;print(tomllib.load(open('$LOCAL','rb'))['aws']['$1'])"; }
REGION=$(value region)
TABLE=$(value table)

if aws dynamodb describe-table --table-name "$TABLE" --region "$REGION" >/dev/null 2>&1; then
    echo "$TABLE is already there"
    exit 0
fi
aws dynamodb create-table --region "$REGION" --table-name "$TABLE" \
    --attribute-definitions AttributeName=id,AttributeType=S \
                            AttributeName=status,AttributeType=S \
                            AttributeName=rank,AttributeType=N \
    --key-schema AttributeName=id,KeyType=HASH \
    --billing-mode PAY_PER_REQUEST \
    --global-secondary-indexes '[{"IndexName":"by-status",
        "KeySchema":[{"AttributeName":"status","KeyType":"HASH"},
                     {"AttributeName":"rank","KeyType":"RANGE"}],
        "Projection":{"ProjectionType":"ALL"}}]' \
    --query 'TableDescription.TableStatus' --output text
aws dynamodb wait table-exists --table-name "$TABLE" --region "$REGION"
echo "$TABLE is ready"
