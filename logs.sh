┌──(e1essar㉿e1essar)-[~/Desktop/tls-crypto-gost-vpn-proto/build]
└─$ ./server --port 4433 
Starting server on port 4433 with cipher 'any'
Configuring GOST cipher list: GOST2012-MAGMA-MAGMAOMAC:GOST2012-KUZNYECHIK-KUZNYECHIKOMAC:LEGACY-GOST2012-GOST8912-GOST8912:IANA-GOST2012-GOST8912-GOST8912:GOST2001-GOST89-GOST89
Server listening on port 4433...
Negotiated cipher: GOST2012-MAGMA-MAGMAOMAC

[Server] Received HTTP request from client:
GET http://example.com/ HTTP/1.1
Host: example.com
User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8
Accept-Language: en-US,en;q=0.5
Accept-Encoding: gzip, deflate
Connection: keep-alive
Upgrade-Insecure-Requests: 1
If-Modified-Since: Mon, 13 Jan 2025 20:11:20 GMT
If-None-Match: "84238dfc8092e5d9c0dac8ef93371a07:1736799080.121134"


[Server] Connecting to target host: example.com
[Server] Sending response to client (250 bytes):
HTTP/1.1 304 Not Modified
Content-Type: text/html
Last-Modified: Mon, 13 Jan 2025 20:11:20 GMT
ETag: "84238dfc8092e5d9c0dac8ef93371a07:1736799080.121134"
Cache-Control: max-age=1535
Date: Thu, 19 Jun 2025 12:05:59 GMT
Connection: keep-alive

--------------------------------------------------------------------------------------------

┌──(e1essar㉿e1essar)-[~/Desktop/tls-crypto-gost-vpn-proto/build]
└─$ ./client --host 127.0.0.1 --port 4433
Starting client to 127.0.0.1:4433 with cipher 'any'
Configuring GOST cipher list: GOST2012-MAGMA-MAGMAOMAC:GOST2012-KUZNYECHIK-KUZNYECHIKOMAC:LEGACY-GOST2012-GOST8912-GOST8912:IANA-GOST2012-GOST8912-GOST8912:GOST2001-GOST89-GOST89
Negotiated cipher: GOST2012-MAGMA-MAGMAOMAC
Client listening on localhost:8080 for HTTP requests...

[Client] Received HTTP request from browser:
GET http://example.com/ HTTP/1.1
Host: example.com
User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8
Accept-Language: en-US,en;q=0.5
Accept-Encoding: gzip, deflate
Connection: keep-alive
Upgrade-Insecure-Requests: 1
If-Modified-Since: Mon, 13 Jan 2025 20:11:20 GMT
If-None-Match: "84238dfc8092e5d9c0dac8ef93371a07:1736799080.121134"


[Client] Received response from server (250 bytes):
HTTP/1.1 304 Not Modified
Content-Type: text/html
Last-Modified: Mon, 13 Jan 2025 20:11:20 GMT
ETag: "84238dfc8092e5d9c0dac8ef93371a07:1736799080.121134"
Cache-Control: max-age=1535
Date: Thu, 19 Jun 2025 12:05:59 GMT
Connection: keep-alive