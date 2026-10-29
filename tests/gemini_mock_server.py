import json
from http.server import BaseHTTPRequestHandler, HTTPServer

PROJECT = {
    "purpose": "Seed 52345 regression project",
    "architecture": "C++20 executable",
    "rationale": "Deterministic fixture",
    "files": [
        {"path": "main.cpp", "content": '#include <iostream>\n#include <string>\nint main(int argc,char** argv){if(argc!=2||std::string(argv[1])!="--selftest")return 2;std::cout<<"SELFTEST_OK";return 0;}'},
        {"path": "CMakeLists.txt", "content": "cmake_minimum_required(VERSION 3.21)\nproject(research_candidate LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\nadd_executable(research_candidate main.cpp)"},
    ],
}

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        self.rfile.read(n)
        body = {"choices": [{"finish_reason": "stop", "message": {"content": json.dumps(PROJECT)}}]}
        raw = json.dumps(body).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def log_message(self, *_):
        pass

HTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
