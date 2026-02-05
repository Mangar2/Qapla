import json
import subprocess
import sys

def test_mcp():
    process = subprocess.Popen(
        [r'build\default\qapla-engine-tester.exe', '--mcp'],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )

    def send_cmd(method, params=None, id=1):
        cmd = {
            "jsonrpc": "2.0",
            "method": method,
            "id": id
        }
        if params:
            cmd["params"] = params
        
        msg = json.dumps(cmd)
        print(f"Sending: {msg}")
        process.stdin.write(msg + "\n")
        process.stdin.flush()

    def read_resp():
        line = process.stdout.readline()
        if line:
            print(f"Received: {line.strip()}")
            return json.loads(line)
        return None

    try:
        # 1. Initialize
        send_cmd("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "test-client", "version": "1.0.0"}
        }, 1)
        read_resp()

        # 2. List tools
        send_cmd("tools/list", id=2)
        read_resp()

        # 3. Manage engines - list
        send_cmd("tools/call", {
            "name": "manage_engines",
            "arguments": {"command": "list"}
        }, 3)
        read_resp()

        # 4. Manage engines - add a test engine
        send_cmd("tools/call", {
            "name": "manage_engines",
            "arguments": {
                "command": "add",
                "name": "TestEngine",
                "engine_cmd": "stockfish.exe",
                "engine_tc": "40/1"
            }
        }, 4)
        read_resp()

        # 5. Manage engines - list again to see if it's there
        send_cmd("tools/call", {
            "name": "manage_engines",
            "arguments": {"command": "list"}
        }, 5)
        read_resp()

        # 5b. Manage engines - details
        send_cmd("tools/call", {
            "name": "manage_engines",
            "arguments": {"command": "details", "name": "TestEngine"}
        }, 52)
        read_resp()

        # 5c. List tools again - to see if (Available: TestEngine) appears in schema
        send_cmd("tools/list", id=53)
        read_resp()

        # 6. Call 'test' tool with the new engine
        send_cmd("tools/call", {
            "name": "test",
            "arguments": {
                "engines": "TestEngine",
                "test_numgames": 1
            }
        }, 6)
        # Testing might take a bit, but it should fail if stockfish.exe doesn't exist, 
        # which is fine for verifying the flow.
        read_resp()

    finally:
        send_cmd("exit", id=99)
        process.terminate()

if __name__ == "__main__":
    test_mcp()
