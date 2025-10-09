"""
This is a demonstration of the challenge validation process 
used by AllStarLink (IAX2).

Copyright (C) 2025, Bruce MacKinnon KC1FSZ
"""
import base64
import hashlib
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

print("Public key size (bit length of the modulus)", public_key.key_size)
print("Public key size (k)", int(public_key.key_size / 8))
print("Public key n", public_key.public_numbers().n)
print("Public key e", public_key.public_numbers().e)

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
print("[1] AUTHREP signature validated, all good!")

# -----------------------------------------------------------------------------
# Doing the signature verification manually. 

k = 128
# We start off with the SHA1 hash of the original message:
M = hashlib.sha1(rsa_challenge.encode("utf-8")).digest()

# Build the encoded message
# EME-PKCS1-V1_5-ENCODE (M, emLen)
# https://datatracker.ietf.org/doc/html/rfc2437#section-9.1.2.1
EM_len = k - 1
EM = bytearray()
PS_len = EM_len - len(M) - 2
EM.append(2)
# TODO: The padding should be random!! Using a fixed value for now
for i in range(0, PS_len):
    EM.append(88)
EM.append(0)
EM = EM + M
assert len(EM) == k - 1

# Get the message representation
# m = OS2IP (EM)
# https://datatracker.ietf.org/doc/html/rfc2437#section-4.2
m = 0
place_value = 1 << (8 * (len(EM) - 1))
for i in range(0, len(EM)):
    m = m + (place_value * int(EM[i]))
    place_value = place_value >> 8

# Get the signature integer representation 
S = rsa_challenge_result
s = 0
for i in range(0, len(S)):
    # Valid of digit in base-256 numbering
    place_value = 256 ** (len(S) - i - 1)
    s += (place_value * int(S[i]))

# Get the message representation from the signature
n = public_key.public_numbers().n
e = public_key.public_numbers().e
print("Slow math ...")
m_prime = (s ** e) % n

# Convert back to string 
work_m_prime = int(m_prime) 
M_prime = bytearray()
# Here we are working from LSB to MSB. 
for i in range(0, k - 1):
    digit = int(work_m_prime % 256)
    if i < len(M):
        # Pull off the right most byte (LSB)
        M_prime.append(digit)
    else:
        break
    # Shift right
    work_m_prime = work_m_prime >> 8
M_prime.reverse()
assert(M == M_prime)
print("[2] AUTHREP signature validated, all good!")
