import http.server
import ssl
import json
import urllib

class MyHttpRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        query = urllib.parse.urlparse(self.path).query
        query_components = urllib.parse.parse_qs(query)
        taken = int(self.headers['Authorization'])
        # post_data = self.rfile.read(content_length)
        # requestData = json.loads(post_data)
        print(taken)
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        response = {"code": 0, "message": "null", "data": {"user_id": taken}}
        self.wfile.write(json.dumps(response).encode())
server_address = ('', 8000)
httpd = http.server.HTTPServer(server_address, MyHttpRequestHandler)
httpd.socket = ssl.wrap_socket(httpd.socket, 
                               certfile='cert.pem', 
                               keyfile='key.pem', 
                               server_side=True)

httpd.serve_forever()
