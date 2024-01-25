EchoLink Protocol Information 
==============================

As far as I can tell the EchoLink protocol isn't officially documented. Maybe I'm missing something. This page attempts to fill in the details. My 
notes are based on examinations of EL packet captures and a review of the various open source EchoLink implementations in GitHub. I 
am not affiliated with the EchoLink team and I have no inside information.  Please take these notes for what they are 
worth - just one random ham's observations.  Do your own research.

Notes like these may make it possible for others to start tinkering around in EchoLink space. If you do so, **please proceed
with caution.** None of us pay when we use EchoLink, so I assume that there are many volunteer hours going on behind the scenes. 
The last thing anyone needs is an accidental denial-of-service incident on the EL network.

Here are the links to the two GitGub projects that I've studied:

* Echolib: https://github.com/sm0svx/svxlink/tree/master/src/echolib
* TheBridge: https://github.com/wd5m/thebridge-1.09

The EL protocols draw heavily on VoIP technology, specifically the RTP and RTCP. Documentation of these two standards helps a lot, but should not be taken too literally as the EL standards do things a bit differently in a few places.

## High-Level Protocol Flow Notes

As has been documented in several places, there are two distinct protocol flows that make up the EL system:
* A client/server interaction with the EchoLink Servers (TCP port 5200).
* A peer-to-peer "QSO" interaction between EchoLink nodes:
  - UDP port 5198 is used to carry audio traffic (and a bit of out-of-band texting) using the RTP protocol.
  - UDP port 5199 is used to carry session control information using something very similar to the RTCP protocol.

The topic of EchoLink Proxies will be put aside for now - more on this later.

Before getting into the details I'll provide my understanding of the high-level message flow that is used
during an EL QSO.  EchoLink is a peer-to-peer technology so it's probably not correct to think about "clients" 
and "servers" for most in this flow, so instead I will use the terms "Station A" and "Station B," assuming that 
Station A is the originator of the call. From tracing a QSO using the official EchoLink client I can see the 
following flow pattern:

1. Station A logs into the network by interacting with the EchoLink Server over TCP 5200.  This 
step may not need to happen on every QSO since the logged-on status persists for some time.
2. Station A requests the directory information from the EchoLink Server to determine the 
status of the target node and its current IP address.
3. Station A sends an RTCP SDES packet with the identification information to Station B on the RTCP port, **but using 
the socket that is bound to the RTP port on the local side!**.  I suspect we need to originate data using both UDP 
ports in order for routers to forward return traffic back to both ports.
4. Station A sends the same message again to the RTCP port of Station B using the socket that is bound to the RTCP port.
5. Station A sends an oNDATA packet on the RTP channel.
6. Station A appears to wait at this point.  If nothing happens after 5 seconds then a retry happens by returning to step #3.
7. (**Not completely sure on this**) Station B uses the call-sign provided in the SDES message send in step 4 to contact
the EchoLink Server and validate that the user is authorized to use the network.
8. Station B sends an RTCP SDES packet on the RTCP channel.
9. Station B sends an oNDATA packet on the RTP channel.
10. Station A sends the same RTCP SDES packet from step #3/#4.  (I suspect this is actually the first of a repeating cycle that will continue throughout the life of the connection.)
11. Station A sends an oNDATA packet on the RTP channel from step #5.  (I suspect this is actually the first of a repeating cycle that will continue throughout the life of the connection.)
12. At this point RTP audio packets are moved in both directions.
13. Periodically (every ~10 seconds) both stations send RTCP SDES and RTP oNDATA packets to each other.
14. At the end of the QSO, Station A sends an RTCP BYE packet.

## EchoLink Server Protocol

Things start off with an exchange with the EchoLink Server. The EL Server topology is described in detail in the 
official EL documentation so we won't repeat this information unnecessarily,  The important detail is that 
there are ~4 active EL Servers that synchronize with each other. An EL node can interact with any one of them.

Nodes initiate the interaction by opening a TCP connection to the EL Server on port 5200.  Data will flow in both 
directions on this connection. The protocol used on this TCP connection appears to be ad-hoc and is entirely 
unrelated to VoIP. The protocol is request/response and appears to be "disconnect delimited" - meaning that 
the server sends a response to the client and then disconnects.

The server interaction accomplishes a few things:

* Allows a node to authenticate itself and to tell the rest of the network about its status. Your callsign must 
be pre-validated for this to work. You use your password to authenticate securely.
* Provides the ability to download the entire directory of EchoLink nodes.
* (Speculation) There is likely a way that a Node can request the status of a specific callsign.

Nodes can establish any one of three statuses on the network:
* ONLINE
* BUSY
* OFF

(**I'm not completely sure of this detail**) Connecting to the EchoLink Server, authenticating, and establishing 
an ONLINE or BUSY status puts the node in "logged in" state for some period of time (**what is the timeout?**).  
This is significant since other nodes on the network will check this status before accepting a QSO from a 
requesting station. Essentially, the EchoLink server is providing a free authentication service for the rest 
of the peer-to-peer network. This must be a lot of volunteer hours behind the scenes for this to work.

### Notes About Password Security

There appear to be two message formats supported by the EL network:
* Old/insecure, wherein the password is transmitted to the server in plain text.
* New/secure, where the server provides an RSA public key and the client encrypts is authentication request.  

Unfortunately, I have not been able to figure out the details of the "secure" exchange yet so all of my research 
is based on the insecure method. (**I'd be very happy to get some information here.**)

### ONLINE/BUSY Message Status Format 

The messages used to authenticate and establish the ONLINE or BUSY status are the same.  Packet format is as follows:

* One byte: 0x63 (lower-case l)
* The callsign
* Two bytes: 0xAC 0xAC
* The password
* One byte: 0x0D 
* The word "ONLINE" or "BUSY" depending on the desired status
* A string identifying the client/version
* One byte: Open paren
* 5 bytes: The local time in HH:MM format, following a 24-hour clock.
* One byte: Close paren
* One byte: 0x0D 
* The location string
* One byte: 0x0D 




## EchoLink QSO Protocol




RTCP SDES Packet Format
-----------------------

[This wiki](https://en.wikipedia.org/wiki/RTP_Control_Protocol) has some relevant background. Each RTCP packet contains a 4-byte header:

* Byte 0 [7:6] - Version, which is always 3.
* Byte 0 [5] - Padding, which is always set to 0.
* Byte 0 [4:0] - Reception report count, which is always set to 0.
* Byte 1 - Packet type.  Always set to 0xC9 (201 = "Receiver Report" per official documentation).  *But this is strange because the content appears to be more consistent with an SDES packet.*  I also note that TheBridge is using 0xC9 in the DES messages that it generates. (More research needed here.)
* Bytes 2-3 - Packet length as a 16-bit integer, most significant byte first.  This is always set to 1 from what I can see.
first. 

The RTCP packets are always padded in order to achieve a packet length that is evenly divisible by 4.  The very last byte of the (padded) packet contains the number of bytes that were added to achieve the padding goal (and that byte is considered to be part of the pad).  One strange thing is that a four-byte pad is added if the message was already divisible by 4, presumably to avoid corrupting the original message by writing the "0" at the last position.  So presumably the valid packet endings look like one of these (in order of transmission/receipt):

* 01
* 00 02
* 00 00 03
* 00 00 00 04

The content of this packet is a variable-length list of tokens.  Each token contains an "SDES type" that indicates the meaning of the token.

Before this repeating pattern, the RTCP packet content starts with a 32-bit SSRC (most significant byte first).  From looking at packet captures and reviewing TheBridge code, it doesn't appear that this number is very important.  For packets generated by the standard EchoLink client the value is set to 0 all the time. TheBridge code generates a non-zero SSRC, but I'm not sure it has any significance.  In TheBridge code we see a comment ([conference.c line ~1330](https://github.com/wd5m/thebridge-1.09/blob/master/src/conference.c#L1329C7-L1329C63)) that says _"We'll never login, set a phoney NodeID so SF will run."_

The SSRC is followed by 8 bytes as follows:

* Two bytes: E1 CA which seem to indicate that the packet contains SDES informatin (the PT field notwithstanding).
* Two bytes of packet length information represented as a 16-bit integer with the most significant byte first.  This length is represented as the number of 4-byte chunks NOT INCLUDING the 12 byte header.  So, for example, if the total length of the packet is 112 bytes we'd see (112 - 12) / 4 = 25 or "00 19" hex.
* Four bytes of the SSRC represented as a 23-bit integer with the most significant byte sent first.  This is alway zero for the EchoLink client but non-zero for the ECHOTEST station (see below).

Then begins a repeating pattern:

* One byte SDES type.
* One byte of length information.
* Variable-length content, without any special termination.

The termination of this pattern requires some specific handling.  From the [RFC 1889](https://www.freesoft.org/CIE/RFC/1889/23.htm) documentation: _"The list of items in each chunk is terminated by one or more null octets, the first of which is interpreted as an item type of zero to denote the end of the list, and the remainder as needed to pad until the next 32-bit boundary. A chunk with zero items (four null octets) is valid but useless."_

The SDES types are as follows (per [TheBridge source code in rtp.h](https://github.com/wd5m/thebridge-1.09/blob/master/inc/rtp.h#L88):

* 0 - END
* 1 - CNAME
* 2 - NAME
* 3 - EMAIL Address
* 4 - PHONE
* 5 - Location 
* 6 - Tool
* 7 - Note
* 8 - PRIV

Here are some notes about how each of these tokens is set by the official EchoLink client (in this order).  From what I can tell, the EchoLink client is using the SDES types its own way and the official RFC SDES definitions shown above are not relevant.

* A type 1 token is sent with the value "CALLSIGN"
* A type 2 token is sent that contains the FCC callsign (padded with some spaces) and the user's real name.  So in my case I see:

        KC1FSZ         Bruce MacKinnon

* A type 3 token is sent with the value "CALLSIGN" again.
* A type 4 token is sent with the ASCII/hex representation of the SSRC identifier being used by the EchoLink client for this session.  This would always be exactly 8 characters since the SSCR is a 32-bit integer.
* A type 6 token with a text description of the client version.  EchoLink is currently sending "E2 3 121".
* A type 8 token with an unknown meaning. The values sent by the official EchoLink client are consistent over time.  I see these bytes being sent (in hex), which spells "<01>P5198" which is likely a reference to the UDP port number that is used for RTP traffic.

        01 50 35 31 39 38

* A type 8 token with an unknown meaning (**does anyone know?**). The values sent by the official EchoLink client are consistent over time. Is see these byte being sent (in hex):

        01 44 30

Finally, optional padding is added to fill up to the next 4-byte boundary (per SDES termination requirement), and then the entire packet is padded again following the RFC requirement.

This visual provides a helpful reference:

![](packet-2.png)

Some notes:

* There are some UDP header bytes at the start that are not relevant.
* The purple box and then four bytes immediately following the purple box have unknown meaning.
* The red boxes show the SDES type and token length.
* The blue bars represent 32-bit boundaries.  This helpful because it shows how the *last* token is being padded up to the next boundary (none of the other tokens do this).  The extra padding is shown in the green boxes.  The 32-bit boundaries don't seem to matter otherwise.
* The entire packet is padded with 4 bytes, with an 04 written in the very last position of the packet.

Here are some notes about how the ECHOTEST station is using the fields in the RTCP packet.

The ECHOTEST station is using a non-zero SSRC that looks like this:

        00 00 27 0F

The SSRC is followed by 8 bytes of unknown meaning.  I see the ECHOTEST station sending this:

        E1 CA 00 16 00 00 27 0F

So clearly the last four bytes are repeating the SSRC.

Then begins the repeating pattern:

* One byte SDES type
* One byte of length information
* Variable-length content, without any special termination.

The ECHOTEST station is sending the following tokens at connection initiation:

* A type 1 token is sent with the value "CALLSIGN"
* A type 2 token is sent that contains some descriptive text:

        *ECHOTEST*  (Conference  [7]) CONF

* A type 3 token is sent with the value "CALLSIGN"
* A type 4 token with the time in HH:MM format.
* A type 6 token with the version of the software "thebridge V1.06" in this case.

The usual mechanics of padding out the last token and then padding the entire packet is observed.

RTCP BYE Packet Format
----------------------

The EchoLink client sends this RTCP packet when the user presses the disconnect button to drop the connection.  

A standard RTCP header is sent which matches the RTCP-RR format. It's a bit strange because the official RTCP document suggest that the "packet type" (PT) field be set to 0xCB to indicate a BYE packet, but EchoLink doesn't seem to work like that.  The EchoLink BYE packet has the usual 0xC9 value and the BYE is encoded in the body of the RTCP packet (immediately after the header).

Following the RTCP header, these 8 bytes are sent:

        E1 CB 00 04 00 00 00 00

It would appear that the first four bytes mean "BYE" and the second four bytes are the SSRC (unused by the official EchoLink client).

This is followed by some of unknown meaning.  The standards documents suggest that this contains some arbitrary text with the "reason for leaving."  The text is preceded by a one-byte length.The EchoLink client sends: "jan2002" in this reason text.

And then the usual mechanics of padding out the entire packet to a 32-bit boundary is observed.

Here is an example capture:

![](packet-3.png)

* There are some UDP header bytes at the start that are not relevant.
* The red box shows the 4-byte BYE message followed by what is presumed to be a 4-byte SSRC identifier.
* The "07" is the length of the text.
* You can see that 4 bytes were added to the end of the packet.

oNDATA Packet Format
--------------------

These packets are sent across the same UDP socket as the RTP audio packets. These packets are sent by the client every ~10 seconds which suggests a keep-alive mechanism.

These packets contain no header information and appear to be plain text with tokens delimited with \r.  The text is also null terminated.  For example:

![](packet-4.png)

* The blue bar indicates the start of the packet.  The bytes before are the UDP/IP header which can be ignored for this analysis.
* The official EchoLink client has 4 additional bytes following the null termination of the text.  These four bytes contain the SSRC, encoded in a 32-bit integer with the most significant byte sent first.
        
RTP Packet Format 
-----------------

Each RTP packet is 144 bytes in length.

Each RTP packet contains a 12-byte header.  The use of this header is very close to the "official" RTP specification.  As usual, we number the bits from LSB [0] to MSB [7], which is different from some other standards documents.

* Byte 0 [7:6] - Version, which is always 3.
* Byte 0 [5] - Padding, which is always set to 0.
* Byte 0 [4] - Extension, which is always set to 0.
* Byte 0 [3:0] - CSRC Count, which is always set to 0 since this feature isn't used.
* Byte 1 [7] - Marker, which is always 0.
* Byte 1 [6:0] - Payload type, which is always 3.  This is consistent with [RFC 3551](https://datatracker.ietf.org/doc/html/rfc3551#section-6).
* Bytes 2-3 - Sequence number represented by a 16-bit integer with the most-signficant 8 bits sent first.
* Bytes 4-7 - Timestamp, which is always set to 0.
* Bytes 8-11 - SSRC identifier, represented by a 32-bit integer with the most-significant 8 bits sent first.  This is intended to uniquely identify the origin of the stream.  There is more discussion of this below.

Following the 12-byte RTP header, exactly 4 GSM frames are encoded. Each
frame is 33 bytes in length.  These frames are packed in chronological order 
with the earliest of the 4 packets in bytes 12-44, the next in bytes 45-77, etc.

The encoding of the GSM frame itself is tricky.  Please consult 
[RFC 3551](https://datatracker.ietf.org/doc/html/rfc3551#section-4.5.8.1) for
details, and pay particular attention to section 4.5.8.1.  This encoding
puts a 4-bit fixed signature 0b1101 in the MSB nibble of the first byte to pad
out the GSM packet to 33 bytes (there are only 260 bits of real information, 
which is only 32.5 bytes).  

The four-bit 0b1101 signature makes it easy to sanity check the RTP packets since you can see them spaced at 33-byte intervals throughout a capture trace:

![packet](packet-1.PNG)

From this example it looks like there is an 0xda at the start of each GSM frame, but that's a coincidence - the "a" part isn't fixed.

GSM CODEC
---------

EchoLink uses GSM 06.10 "full-rate" audio coding. The complete description can be found in the [European Telecommunications Standards Institute specification document](https://www.etsi.org/deliver/etsi_EN/300900_300999/300961/08.00.01_40/en_300961v080001o.pdf).

An embedded-friendly implementation of this CODEC has been produced for this project.

Notes on the RTP SSRC Identifier
--------------------------------
(To follow)
