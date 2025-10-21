# AllStarLink Protocol Information 

[By Bruce MacKinnon, KC1FSZ](https://www.qrz.com/db/kc1fsz)

## Network Information

IAX2 runs on UDP port 4569. It appears that this is the only 
port that needs to be open from the outside.

## Node Registration

A JSON message is sent to the AllStarLink registry
when a node starts up to allow the 
network to know the IP address that the node is running
on. It's likely that this message is sent out on a 
periodic basis to keep the registration active.

The format of this JSON message doesn't appear to be directly
documented, but it can be determined from looking at the 
source code.

Here's an example of a command that will send the 
properly formatted message:
```
curl -X POST "register.allstarlink.org" \
     -H "Content-Type: application/json" \
     -d '{ 
    "port": 7777,
    "data": { 
        "nodes": { 
            "61057": { 
                "node": "61057", 
                "passwd": "xxxxxx", 
                "remote": 0 
            } 
        } 
    } 
} 
' \
     --trace allstarlink.log
```
Here's the JSON message (the important part):
```json
{
    "port": 7777,
    "data": {
        "nodes": {
            "61057": {
                "node": "61057",
                "passwd": "xxxxxx",
                "remote": 0
            }
        }
    }
}
```

## Message Flows

### New Call from AllStarLink Phone Portal

* (From Caller) Full Packet, Type=Control(4), Subclass=NEW(1)
* (To Caller)  Full Packet, Type=Control(4), Subclass=CALLTOKEN(40)
* (From Caller) Full Packet, Type=Control(4), Subclass=NEW(1)
* (To Caller) ACK
* (To Caller) AUTHREQ
* (From Caller) AUTHREP
* (To Caller) ACK
* (To Caller) ACCEPT
* (To Caller) Full Packet, Type=Control(4), Subclass=RINGING(3)
* (From Caller) Full Packet, Type=IAX(6), Sublcass=ACK(4)
* (From Caller) Full Packet, Type=IAX(6), Sublcass=ACK(4)
* (From Caller) Full Packet, Type=Voice(2)
* (To Caller) Type=IAX(6), Sublcass=ACK(4)
* (From Caller) Mini voice packet
* (From Caller) Mini voice packet
* ...
* (To Caller) Full Packet, Type=Control(4), Subclass=ANSWER(4)
* (To Caller) Full Packet, Type=Control(4), Subclass=Stop Sounds(255)

## Message Format/Semantics

# Voice Packet (Full)

G.711 μ-law payload contains the actual 8-bit μ-law encoded audio samples. The size of 
this payload depends on the packetization interval.  20ms of audio at 8kHz a sampling
rate is 160 bytes.

# AUTHREQ

Sent from the server to the client to request authentication. Contains a 9-digit challenge,
which is a random number.

# AUTHREP

Send from the client to the server in response to an AUTHREQ.

Contains the RSA signature for the server's challenge. This is Base-64 encoded. The 
signature will be validated by the server using a public RSA key. Digest hash is SHA1.

Sample code to illustrate the mechanics:

```python
"""
This is a demonstration of the challenge validation process 
used by AllStarLink (IAX2).

Copyright (C) 2025, Bruce MacKinnon KC1FSZ
"""
import base64
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives import serialization
from cryptography.exceptions import InvalidSignature

"""
From RFC-5456, Section 8.6.16:

The purpose of the RSA RESULT information element is to offer an RSA
response to an authentication CHALLENGE.  It carries the UTF-8-
encoded challenge result.  The result is computed as follows: first,
compute the SHA1 digest [RFC3174] of the challenge string and second,
RSA sign the SHA1 digest using the private RSA key as specified in
PKCS #1 v2.0 [PKCS].  The RSA keys are stored locally.

Upon receiving an RSA RESULT information element, its value must be
verified with the sender's public key to match the SHA1 digest
[RFC3174] of the challenge string.
"""

# The RSA public key is provided in the ASL3 installation. On the Pi
# appliance distribution it is located at:
#   /usr/share/asterisk/keys/allstar.pub
# The public key is in PEM format:
public_key_pem = "-----BEGIN PUBLIC KEY-----\n\
MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCu3h0BZQQ+s5kNM64gKxZ5PCpQ\n\
9BVzhl+PWVYXbEtozlJVVs1BHpw90GsgScRoHh4E76JuDYjEdCTuAwg1YkHdrPfm\n\
BUjdw8Vh6wPFmf3ozR6iDFcps4/+RkCUb+uc9v0BqZIzyIdpFC6dZnJuG5Prp7gJ\n\
hUaYIFwQxTB3v1h+1QIDAQAB\n\
-----END PUBLIC KEY-----\n"
public_key = serialization.load_pem_public_key(public_key_pem.encode("utf-8"))

# The RSA challenge string is a 9-digit number created randomly by 
# the server creating the AUTHREQ message. Here is an example that 
# was captured from the network during an actual connection:
rsa_challenge = "570639908"

# The RSA signature is created by the client attempting to connect.
# This is created using the client's private key. The signature
# is sent back in the AUTHREP message. Here is an example that
# was captured from the network during an actual connection:
rsa_challenge_result_base64 = "ZanWw1+Wx5TWWX6g4890bmnflMgk8ZyyRdjINenNmzq3eYWfPMpcfMFIrHfX0gxOzGeNflcbOqr1m6GMnCoE92h+fMlIEZceUuCZXh+GZ4ywiy3RJluvE/Cj/vkh5Af38jb5PjT2dJB/HMZ8mSZ7qDQgcjjotNRmWVGhAMte9Nc="

# The signature in the AUTHREP message is actually a base-64 encoding 
# of the signature:
rsa_challenge_result = base64.b64decode(rsa_challenge_result_base64.encode("utf-8"))

# Here is where the actual validation happens:
public_key.verify(rsa_challenge_result,
    rsa_challenge.encode("utf-8"), 
    padding.PKCS1v15(), 
    hashes.SHA1())

# If we get here the validation is good, otherwise an exception is raised
print("AUTHREP signature validated, all good!")
```
