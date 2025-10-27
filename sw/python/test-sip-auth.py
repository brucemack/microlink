"""
This is a demonstration of the HTTP Digest authentication process 
used in SIP negotiation.

Copyright (C) 2025, Bruce MacKinnon KC1FSZ
"""
import hashlib
import base64

"""
This is a demonstration of how to validate a digest response 
to prove that the client knows a secret password.

Assume this is the challenge response header sent from the server 
to the client inside of a 401:

WWW-Authenticate: Digest realm="asterisk",
  nonce="1761157360/8582866c17ef9e4cfd024254e37cf1e8",
  opaque="50d273e02ab5c6c3",algorithm=MD5,qop="auth"

And assume this is the new request sent from the client to the 
server. The goal is to validate the contents of the "response"
attribute here.

Authorization: Digest username="1001", realm="asterisk", 
  nonce="1761157360/8582866c17ef9e4cfd024254e37cf1e8", 
  qop=auth, cnonce="/68T/KlfvEd3p7T", nc=00000001, 
  opaque="50d273e02ab5c6c3", 
  uri="sip:61057@192.168.8.102:5060;user=phone", 
  response="30ffb4415b99d73da5ef3badee2781d1", algorithm=MD5
"""

def MD5(a):
    md5_hash = hashlib.md5()
    md5_hash.update(a.encode("utf-8"))
    return md5_hash.hexdigest()

# Here is the secret that the client has:
password="1001"

# All of this is contained in the client's request (no 
# secrets here):
realm="asterisk"
username="1001"
uri="sip:61057@192.168.8.102:5060;user=phone"
nonce="1761157360/8582866c17ef9e4cfd024254e37cf1e8"
nc="00000001"
cnonce="/68T/KlfvEd3p7T"
qop="auth"
response="30ffb4415b99d73da5ef3badee2781d1"

# Here we are following the steps outline in RFC 2616 section 3.2.2.1.
# Please see https://datatracker.ietf.org/doc/html/rfc2617#section-3.2.2.1

# A1 = username-value ":" realm-value ":" passwd-value
a1 = username + ":" + realm + ":" + password
# A2 = Method ":" digest-uri-value
a2 = "INVITE" + ":" + uri
secret = MD5(a1) + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + MD5(a2)

assert(MD5(secret) == response)

print("Secret is validated successfully!")

