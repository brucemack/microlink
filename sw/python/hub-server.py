import time
import requests
import socket
import random
from enum import Enum
import base64
import hashlib
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives import serialization
from cryptography.exceptions import InvalidSignature

def is_full_frame(frame):
    return frame[0] & 0b10000000 == 0b10000000

def get_full_source_call(frame):
    return ((frame[0] & 0b01111111) << 8) | frame[1]

def get_full_r_bit(frame):
    return frame[2] & 0b10000000 == 0b10000000

def get_full_dest_call(frame):
    return ((frame[2] & 0b01111111) << 8) | frame[3]

def get_full_timestamp(frame):
    return (frame[4] << 24) | (frame[5] << 16) | (frame[6] << 8) | frame[7]

def get_full_outseq(frame):
    return frame[8]

def get_full_inseq(frame):
    return frame[9]

def get_full_type(frame):
    return frame[10]

def get_full_subclass_c_bit(frame):
    return frame[11] & 0b10000000 == 0b10000000

def get_full_subclass(frame):
    return frame[11] & 0b01111111

def make_call_token():
    # TODO: RANDOMIZE
    return "1759883232?e4b9017e102c1f831e6db6ab1bc85ebce1ea240e".encode("utf-8")

def make_information_element(id: int, content):
    result = bytearray()
    result += id.to_bytes(1, byteorder='big')
    result += len(content).to_bytes(1, byteorder='big')
    result += content
    return result

def encode_information_elements(ie_map: dict): 
    result = bytearray()
    for key in ie_map.keys():
        if not isinstance(key, int):
            raise Exception("Type error")
        result += make_information_element(key, ie_map[key])
    return result

def decode_information_elements(data: bytes):
    """
    Takes a byte array containing zero or more information elements
    and unpacks it into a dictionary. The key of the dictionary is 
    the integer element ID and the value of the dictionary is a byte
    array with the content of the element.
    """
    result = dict()
    state = 0
    working_id = 0
    working_length = 0
    working_data = None
    # Cycle across all data
    for b in data:
        if state == 0:
            working_id = b
            state = 1
        elif state == 1:
            working_length = b 
            working_data = bytearray()
            if working_length == 0:
                result[working_id] = working_data
                state = 0
            else:
                state = 2
        elif state == 2:
            working_data.append(b)
            if len(working_data) == working_length:
                result[working_id] = working_data
                state = 0
        else:
            raise Exception()
    # Sanity check - we should end in the zero state
    if state != 0:
        raise Exception("Data format error")
    return result

def is_NEW_frame(frame):
    return is_full_frame(frame) and \
        get_full_type(frame) == 6 and \
        get_full_subclass_c_bit(frame) == False and \
        get_full_subclass(frame) == 1

def is_ACK_frame(frame):
    return is_full_frame(frame) and \
        get_full_type(frame) == 6 and \
        get_full_subclass_c_bit(frame) == False and \
        get_full_subclass(frame) == 4

def make_CALLTOKEN_frame(source_call: int, dest_call: int, timestamp: int, 
    out_seq: int, in_seq: int, token):
    result = bytearray()
    result += source_call.to_bytes(2, byteorder='big')
    result[0] = result[0] | 0b10000000
    result += dest_call.to_bytes(2, byteorder='big')
    result[2] = result[2] & 0b01111111
    result += timestamp.to_bytes(4, byteorder='big')
    result += out_seq.to_bytes(1, byteorder='big')
    result += in_seq.to_bytes(1, byteorder='big')
    result += int(6).to_bytes(1, byteorder='big')
    # Subclass
    result += int(40).to_bytes(1, byteorder='big')
    # Information elements
    result += encode_information_elements({ 54: token })
    return result

def make_ACK_frame(source_call: int, dest_call: int, timestamp: int,
    out_seq: int, in_seq: int):
    result = bytearray()
    result += source_call.to_bytes(2, byteorder='big')
    result[0] = result[0] | 0b10000000
    result += dest_call.to_bytes(2, byteorder='big')
    result[2] = result[2] & 0b01111111
    result += timestamp.to_bytes(4, byteorder='big')
    result += out_seq.to_bytes(1, byteorder='big')
    result += in_seq.to_bytes(1, byteorder='big')
    result += int(6).to_bytes(1, byteorder='big')
    # Subclass
    result += int(4).to_bytes(1, byteorder='big')
    return result

def make_AUTHREQ_frame(source_call: int, dest_call: int, timestamp: int,
    out_seq: int, in_seq: int, challenge: str):
    result = bytearray()
    result += source_call.to_bytes(2, byteorder='big')
    result[0] = result[0] | 0b10000000
    result += dest_call.to_bytes(2, byteorder='big')
    result[2] = result[2] & 0b01111111
    result += timestamp.to_bytes(4, byteorder='big')
    result += out_seq.to_bytes(1, byteorder='big')
    result += in_seq.to_bytes(1, byteorder='big')
    result += int(6).to_bytes(1, byteorder='big')
    # Subclass
    result += int(8).to_bytes(1, byteorder='big')
    # Information elements
    result += encode_information_elements({ 
        14: int(4).to_bytes(2, byteorder='big'),
        15: challenge.encode("utf-8"), 
        6: "allstar-sys".encode("utf-8") 
    })
    return result

def make_ACCEPT_frame(source_call: int, dest_call: int, timestamp: int,
    out_seq: int, in_seq: int):
    result = bytearray()
    result += source_call.to_bytes(2, byteorder='big')
    result[0] = result[0] | 0b10000000
    result += dest_call.to_bytes(2, byteorder='big')
    result[2] = result[2] & 0b01111111
    result += timestamp.to_bytes(4, byteorder='big')
    result += out_seq.to_bytes(1, byteorder='big')
    result += in_seq.to_bytes(1, byteorder='big')
    result += int(6).to_bytes(1, byteorder='big')
    # Subclass
    result += int(7).to_bytes(1, byteorder='big')
    # Information elements
    result += encode_information_elements({ 
        9: int(4).to_bytes(4, byteorder='big'),
        56: b'\x00\x00\x00\x00\x00\x00\x00\x00\x04'
    })
    return result

def make_RINGING_frame(source_call: int, dest_call: int, timestamp: int,
    out_seq: int, in_seq: int):
    result = bytearray()
    result += source_call.to_bytes(2, byteorder='big')
    result[0] = result[0] | 0b10000000
    result += dest_call.to_bytes(2, byteorder='big')
    result[2] = result[2] & 0b01111111
    result += timestamp.to_bytes(4, byteorder='big')
    result += out_seq.to_bytes(1, byteorder='big')
    result += in_seq.to_bytes(1, byteorder='big')
    # Type is control
    result += int(4).to_bytes(1, byteorder='big')
    # Subclass
    result += int(3).to_bytes(1, byteorder='big')
    return result

def make_ANSWER_frame(source_call: int, dest_call: int, timestamp: int,
    out_seq: int, in_seq: int):
    result = bytearray()
    result += source_call.to_bytes(2, byteorder='big')
    result[0] = result[0] | 0b10000000
    result += dest_call.to_bytes(2, byteorder='big')
    result[2] = result[2] & 0b01111111
    result += timestamp.to_bytes(4, byteorder='big')
    result += out_seq.to_bytes(1, byteorder='big')
    result += in_seq.to_bytes(1, byteorder='big')
    # Type is control
    result += int(4).to_bytes(1, byteorder='big')
    # Subclass
    result += int(4).to_bytes(1, byteorder='big')
    return result

def make_STOP_SOUNDS_frame(source_call: int, dest_call: int, timestamp: int,
    out_seq: int, in_seq: int):
    result = bytearray()
    result += source_call.to_bytes(2, byteorder='big')
    result[0] = result[0] | 0b10000000
    result += dest_call.to_bytes(2, byteorder='big')
    result[2] = result[2] & 0b01111111
    result += timestamp.to_bytes(4, byteorder='big')
    result += out_seq.to_bytes(1, byteorder='big')
    result += in_seq.to_bytes(1, byteorder='big')
    # Type is control
    result += int(4).to_bytes(1, byteorder='big')
    # Subclass
    result += int(255).to_bytes(1, byteorder='big')
    return result

def current_ms():
    return int(time.time() * 1000)

"""
# Unit test
a = make_CALLTOKEN_frame(1, 8125, 19, 0, 1, make_call_token())
print(len(a), a)

# Unit test - round-trip on information element encoding
d = dict()
d[1] = "111".encode("utf-8")
d[2] = "222".encode("utf-8")
d[3] = "".encode("utf-8")
enc_d = encode_information_elements(d)
d_prime = decode_information_elements(enc_d)
assert(d == d_prime)
quit()
"""

node_id = "61057"
node_password = "microlink"

iax2_port = 4569
reg_url = "https://register.allstarlink.org"
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

class State(Enum):
    IDLE = 1
    NEW1 = 2 
    NEW2 = 3
    RINGING = 4
    IN_CALL = 5

call_id_counter = 1
state = State.IDLE
state_source_call_id = 0
state_call_id = 0
state_call_start_ms = 0
state_call_start_stamp = 0
state_challenge = ""
state_expected_inseq = 0
state_outseq = 0

reg_node_msg = {
    "node": node_id,
    "passwd": node_password,
    "remote": 0
}

reg_msg = {
    "port": 7777,
    "data": {
        "nodes": {
        }
    }
}

reg_msg["data"]["nodes"][node_id] = reg_node_msg
print(reg_msg)

response = requests.post(reg_url, json=reg_msg)
print(response.text)

UDP_IP = "0.0.0.0" 
# Create a UDP socket and bind 
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, iax2_port))
# This prevents the recvfrom() call below from blocking forever
sock.settimeout(0.25)

print(f"Listening on IAX2 port {UDP_IP}:{iax2_port}")

# ---- Main processing loop --------------------------------------------------

while True:

    try:
        frame, addr = sock.recvfrom(1024)
    except socket.timeout:
        continue

    if is_full_frame(frame):

        print("---------", f"Received message from {addr}")        
        print("Full frame", get_full_r_bit(frame), get_full_source_call(frame), get_full_dest_call(frame))
        print("Type", get_full_type(frame), "Subclass", get_full_subclass(frame))
        print("Oseqno", get_full_outseq(frame), "Iseqno", get_full_inseq(frame))

        # Deal with the inbound sequence number tracking.
        # When a NEW is received the inbound sequence counter is reset.
        if is_NEW_frame(frame):
            state_expected_inseq = 1
            state_outseq = 0

        # When an ACK is received we can validate its OSeqno, but we don't move 
        # the expectation forward since the sender isn't incrementing their sequence
        # for an ACK.
        elif is_ACK_frame(frame):
            if not get_full_r_bit(frame):
                if  get_full_outseq(frame) != state_expected_inseq:
                    print("WARNING: Inbound sequence error")

        # For all other frames we validate the sequence number
        # and then move our expectation forward.
        else:
            if not get_full_r_bit(frame):
                if  get_full_outseq(frame) != state_expected_inseq:
                    print("WARNING: Inbound sequence error")
                # Pay attention to wrap
                state_expected_inseq = (get_full_outseq(frame) + 1) % 256
    else:
        print("Mini frame")

    if state == State.IDLE:
        if is_NEW_frame(frame):
            # Get call start information
            state_source_call_id = get_full_source_call(frame)
            state_call_start_stamp = current_ms()
            state_call_start_ms = get_full_timestamp(frame)
            # Send a CALLTOKEN challenge
            state_token = make_call_token()
            # NOTE: For now the call ID is set to 1
            resp = make_CALLTOKEN_frame(1, 
                state_source_call_id,
                state_call_start_ms + (current_ms() - state_call_start_stamp),
                state_outseq, 
                state_expected_inseq,                
                state_token)
            print("Sending CALLTOKEN", resp, state_outseq, state_expected_inseq)
            state_outseq += 1
            sock.sendto(resp, addr)
            state = State.NEW1
        else:
            print("Ignoring unknown message")

    # In this state we are waiting for a NEW with the right CALLTOKEN
    elif state == State.NEW1:
        if is_NEW_frame(frame):

            # Decode the information elements
            ies = decode_information_elements(frame[12:])

            # Make sure we have the right token
            if get_full_source_call(frame) == state_source_call_id and \
                54 in ies and \
                ies[54] == state_token:

                # Generate the unique ID for this call
                state_call_id = call_id_counter
                call_id_counter += 1
                # Generate the authentication challenge data
                state_challenge = "{:09d}".format(random.randint(1,999999999))

                print("Got expected token, starting call", state_call_id)

                # Send ACK
                resp = make_ACK_frame(state_call_id, 
                    state_source_call_id,
                    state_call_start_ms + (current_ms() - state_call_start_stamp),
                    state_outseq, 
                    state_expected_inseq)
                print("Sending ACK", resp, state_outseq, state_expected_inseq)
                sock.sendto(resp, addr)
                # IMPORTANT: We don't move the outseq forward!

                # Send AUTHREQ
                resp = make_AUTHREQ_frame(state_call_id, 
                    state_source_call_id,
                    state_call_start_ms + (current_ms() - state_call_start_stamp),
                    state_outseq, 
                    state_expected_inseq,
                    state_challenge)
                print("Sending AUTHREQ", resp, state_outseq, state_expected_inseq)
                sock.sendto(resp, addr)
                state_outseq += 1                
                state = State.NEW2

            else:
                print("Invalid token")
                state = State.IDLE
        else:
            print("Ignoring unknown message")

    # In this state we are waiting for an AUTHREP
    elif state == State.NEW2:
        if is_full_frame(frame) and \
            get_full_type(frame) == 6 and \
            get_full_subclass_c_bit(frame) == False and \
            get_full_subclass(frame) == 9:

            # Decode the information elements
            ies = decode_information_elements(frame[12:])

            if get_full_source_call(frame) == state_source_call_id and \
               get_full_dest_call(frame) == state_call_id and \
                17 in ies:

                rsa_challenge_result = base64.b64decode(ies[17])

                # Here is where the actual validation happens:
                public_key.verify(rsa_challenge_result,
                    state_challenge.encode("utf-8"), 
                    padding.PKCS1v15(), 
                    hashes.SHA1())

                print("Authenticated!")

                # Send ACK
                resp = make_ACK_frame(state_call_id, 
                    state_source_call_id,
                    state_call_start_ms + (current_ms() - state_call_start_stamp),
                    state_outseq, 
                    state_expected_inseq)
                print("Sending ACK", resp, state_outseq, state_expected_inseq)
                sock.sendto(resp, addr)
                # IMPORTANT: We don't move the outseq forward!

                # Send the ACCEPT
                resp = make_ACCEPT_frame(state_call_id, 
                    state_source_call_id,
                    state_call_start_ms + (current_ms() - state_call_start_stamp),
                    state_outseq, 
                    state_expected_inseq)
                print("Sending ACCEPT", resp, state_outseq, state_expected_inseq)
                sock.sendto(resp, addr)
                state_outseq += 1

                # Send the RINGING
                resp = make_RINGING_frame(state_call_id, 
                    state_source_call_id,
                    state_call_start_ms + (current_ms() - state_call_start_stamp),
                    state_outseq, 
                    state_expected_inseq)
                print("Sending RINGING", resp, state_outseq, state_expected_inseq)
                sock.sendto(resp, addr)
                state_outseq += 1

                state = State.RINGING
                state_timeout = current_ms() + 1000

            else:
                print("AUTHREP error")

    # In this state we are waiting for some time to pass
    elif state == State.RINGING:
        if current_ms() > state_timeout:

            # Send the ANSWER
            resp = make_ANSWER_frame(state_call_id, 
                state_source_call_id,
                state_call_start_ms + (current_ms() - state_call_start_stamp),
                state_outseq, 
                state_expected_inseq)
            print("Sending ANSWER", resp, state_outseq, state_expected_inseq)
            sock.sendto(resp, addr)
            state_outseq += 1

            # Send the STOP_SOUNDS
            resp = make_STOP_SOUNDS_frame(state_call_id, 
                state_source_call_id,
                state_call_start_ms + (current_ms() - state_call_start_stamp),
                state_outseq, 
                state_expected_inseq)
            print("Sending STOP_SOUNDS", resp, state_outseq, state_expected_inseq)
            sock.sendto(resp, addr)
            state_outseq += 1

            state = State.IN_CALL
