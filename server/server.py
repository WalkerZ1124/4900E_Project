from http.server import HTTPServer, SimpleHTTPRequestHandler
import ssl
import os

# A simple HTTP server that serves files from local directory
# This server is intended to be used for testing purposes only

# USAGE:
# generate the cert.pem and server.key files using the following command:
# rename the intended binary to "update.bin" in the bins directory
# openssl req -x509 -newkey rsa:2048 -nodes -keyout server.key -out ca_cert.pem -days 365 -subj "/CN=192.168.0.20"



httpd = HTTPServer(('0.0.0.0', 8000), SimpleHTTPRequestHandler)
os.chdir('bins')
# Create an SSL context
ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ssl_context.minimum_version = ssl.TLSVersion.TLSv1_2
ssl_context.load_cert_chain(certfile='../../server/ca_cert.pem', keyfile='../../server/server.key')

# Wrap the server's socket with the SSL context
httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)

httpd.serve_forever()


