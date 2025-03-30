from http.server import HTTPServer, SimpleHTTPRequestHandler
import ssl
import os

httpd = HTTPServer(('0.0.0.0', 8000), SimpleHTTPRequestHandler)
os.chdir('server/bins')
# Create an SSL context
ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ssl_context.minimum_version = ssl.TLSVersion.TLSv1_2
ssl_context.load_cert_chain(certfile='../../server/cert.pem', keyfile='../../server/server.key')

# Wrap the server's socket with the SSL context
httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)

httpd.serve_forever()


# generate the cert.pem and server.key files using the following command:
# openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes -addext "subjectAltName=IP:192.168.0.83"

