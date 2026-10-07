"""언리얼 에디터 MCP 서버(ModelContextProtocol 플러그인)를 HTTP로 직접 부르는 도구.

AI 클라이언트의 MCP 연결이 실패했을 때도(예: 세션 시작 때 에디터가 꺼져 있었음) 같은 서버를 쓸 수 있다.
파이썬 3 표준 라이브러리만 쓴다.

사용법 (Tools/UnrealAgent 폴더에서, 또는 경로를 붙여 실행):
  python mcp_call.py toolsets                         # 사용할 수 있는 도구 묶음 목록
  python mcp_call.py call <toolset> <tool> [JSON] [--image out.png]
  python mcp_call.py capture out.png                  # 에디터 화면 캡처 (1280×720 PNG)
  python mcp_call.py pie start | pie stop             # 플레이(PIE) 시작·종료
  python mcp_call.py logs [카테고리] [정규식]          # 출력 로그 (기본 카테고리 LogCozyRealm)
  python mcp_call.py livecoding                       # Live Coding 컴파일 (헤더 변경 없는 .cpp만)
  python mcp_call.py raw <method> [JSON]              # MCP 메서드 직접 호출 (예: tools/list)

설정:
  UNREAL_MCP_URL  서버 주소 (기본 http://127.0.0.1:8000/mcp)
  세션 아이디는 이 폴더의 .state/ 에 저장한다 (Git에서 제외).
"""
import base64
import hashlib
import json
import os
import re
import sys
import urllib.error
import urllib.request

URL = os.environ.get("UNREAL_MCP_URL", "http://127.0.0.1:8000/mcp")
STATE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".state")
SESSION_FILE = os.path.join(STATE_DIR, "session_" + hashlib.md5(URL.encode()).hexdigest()[:8] + ".txt")


def _post(body, sid=None, timeout=600):
    headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    if sid:
        headers["Mcp-Session-Id"] = sid
    req = urllib.request.Request(URL, json.dumps(body).encode("utf-8"), headers)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.headers.get("Mcp-Session-Id"), r.read().decode("utf-8", "replace")


def _parse(text):
    """일반 JSON 응답과 SSE(data: ...) 응답을 모두 처리한다."""
    text = text.strip()
    if not text:
        return {}
    if text.startswith("{"):
        return json.loads(text)
    data = [line[5:].strip() for line in text.splitlines() if line.startswith("data:")]
    return json.loads(data[-1]) if data else {"raw": text}


def _new_session():
    sid, _ = _post({"jsonrpc": "2.0", "id": 1, "method": "initialize",
                    "params": {"protocolVersion": "2025-03-26", "capabilities": {},
                               "clientInfo": {"name": "unreal-agent-tools", "version": "1"}}}, timeout=30)
    _post({"jsonrpc": "2.0", "method": "notifications/initialized"}, sid, timeout=30)
    os.makedirs(STATE_DIR, exist_ok=True)
    with open(SESSION_FILE, "w") as f:
        f.write(sid or "")
    return sid


def _session():
    if os.path.exists(SESSION_FILE):
        with open(SESSION_FILE) as f:
            sid = f.read().strip()
        if sid:
            return sid
    return _new_session()


def rpc(method, params, timeout=600):
    body = {"jsonrpc": "2.0", "id": 2, "method": method, "params": params}
    try:
        sid = _session()
        try:
            _, text = _post(body, sid, timeout)
            result = _parse(text)
            if "Unknown session" in json.dumps(result.get("error", "")):
                raise urllib.error.HTTPError(URL, 404, "unknown session", None, None)
            return result
        except urllib.error.HTTPError:
            # 에디터를 다시 켜면 예전 세션 아이디가 무효가 된다 → 새 세션으로 한 번 더
            sid = _new_session()
            _, text = _post(body, sid, timeout)
            return _parse(text)
    except urllib.error.URLError as e:
        sys.exit(f"연결 실패: {URL} ({e.reason}) · 언리얼 에디터가 켜져 있고 MCP 서버가 자동 시작으로 설정됐는지 확인하세요")


def call_tool(toolset, tool, arguments=None, timeout=600):
    return rpc("tools/call", {"name": "call_tool", "arguments": {
        "toolset_name": toolset, "tool_name": tool, "arguments": arguments or {}}}, timeout)


def print_result(result, image_out=None):
    """결과의 글자는 출력하고, 이미지(base64)는 파일로 저장한다. 실패면 종료 코드 1."""
    if "error" in result:
        print(json.dumps(result["error"], ensure_ascii=False))
        sys.exit(1)
    r = result.get("result", result)
    saved = False
    for c in r.get("content", []):
        if c.get("type") == "image":
            if image_out:
                with open(image_out, "wb") as f:
                    f.write(base64.b64decode(c["data"]))
                print("saved", image_out)
                saved = True
        elif c.get("type") == "text":
            t = c["text"]
            m = re.search(r'"data"\s*:\s*"([A-Za-z0-9+/=]{200,})"', t)
            if m and image_out:
                with open(image_out, "wb") as f:
                    f.write(base64.b64decode(m.group(1)))
                print("saved", image_out)
                saved = True
            print(re.sub(r'"data"\s*:\s*"[A-Za-z0-9+/=]{200,}"', '"data":"<image>"', t)[:4000])
    if image_out and not saved:
        print("실패: 결과에 이미지가 없습니다")
        sys.exit(1)
    if r.get("isError"):
        sys.exit(1)


def main(argv):
    if not argv or argv[0] in ("-h", "--help", "help"):
        print(__doc__)
        return
    image_out = None
    if "--image" in argv:
        i = argv.index("--image")
        image_out = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    cmd = argv[0]
    if cmd == "toolsets":
        print_result(rpc("tools/call", {"name": "list_toolsets", "arguments": {}}))
    elif cmd == "call":
        args = json.loads(argv[3]) if len(argv) > 3 else {}
        print_result(call_tool(argv[1], argv[2], args), image_out)
    elif cmd == "capture":
        out = os.path.abspath(argv[1])
        os.makedirs(os.path.dirname(out), exist_ok=True)
        print_result(call_tool("EditorToolset.EditorAppToolset", "CaptureEditorImage"), out)
    elif cmd == "pie":
        if argv[1] == "start":
            print_result(call_tool("EditorToolset.EditorAppToolset", "StartPIE", {
                "options": {"bSimulate": False, "playMode": "PlayMode_InViewPort", "warmupSeconds": 3}}))
        else:
            print_result(call_tool("EditorToolset.EditorAppToolset", "StopPIE"))
    elif cmd == "logs":
        args = {"category": argv[1] if len(argv) > 1 else "LogCozyRealm"}
        if len(argv) > 2:
            args["pattern"] = argv[2]
        print_result(call_tool("EditorToolset.LogsToolset", "GetLogEntries", args))
    elif cmd == "livecoding":
        # 디스크가 느리면 수십 분 걸릴 수 있다 (2026-10-07 E: 드라이브에서 약 20분)
        print_result(call_tool("LiveCodingToolset.LiveCodingToolset", "CompileLiveCoding", timeout=3600))
    elif cmd == "raw":
        print(json.dumps(rpc(argv[1], json.loads(argv[2]) if len(argv) > 2 else {}), ensure_ascii=False)[:8000])
    else:
        sys.exit(f"알 수 없는 명령: {cmd} (python mcp_call.py --help)")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main(sys.argv[1:])
