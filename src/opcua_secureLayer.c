/*
 * opcua_secureChannelLayer.c
 *
 *  Created on: Jan 13, 2014
 *      Author: opcua
 */
#include <stdio.h>
#include <memory.h> // memcpy
#include "opcua_transportLayer.h"
#include "opcua_secureLayer.h"

#define SIZE_SECURECHANNEL_HEADER 12
#define SIZE_SEQHEADER_HEADER 8


/*
 * inits a connection object for secure channel layer
 */
UA_Int32 SL_initConnectionObject(UA_connection *connection)
{

	//TODO: fill with valid information
	UA_ByteString_init(&(connection->secureLayer.localAsymAlgSettings.ReceiverCertificateThumbprint));
	UA_ByteString_copy(&(connection->secureLayer.localAsymAlgSettings.SecurityPolicyUri), UA_String_securityPoliceNone);
	UA_ByteString_init(&(connection->secureLayer.localAsymAlgSettings.SenderCertificate));
	UA_ByteString_init(&(connection->secureLayer.remoteNonce));

	UA_alloc((void**)&(connection->secureLayer.localNonce.data),sizeof(UA_Byte));
	connection->secureLayer.localNonce.length = 1;

	connection->secureLayer.connectionState = connectionState_CLOSED;

	connection->secureLayer.requestId = 0;
	connection->secureLayer.requestType = 0;

	UA_String_init(&(connection->secureLayer.secureChannelId));

	connection->secureLayer.securityMode = UA_SECURITYMODE_INVALID;
	//TODO set a valid start secureChannelId number
	connection->secureLayer.securityToken.secureChannelId = 25;

	//TODO set a valid start TokenId
	connection->secureLayer.securityToken.tokenId = 1;
	connection->secureLayer.sequenceNumber = 1;

	return UA_NO_ERROR;
}

UA_Int32 SL_send(UA_connection *connection, UA_ByteString responseMessage, UA_Int32 type)
{
	UA_UInt32 sequenceNumber;
	UA_UInt32 requestId;
	UA_Int32 pos;
	UA_Int32 sizeAsymAlgHeader;
	UA_ByteString responsePacket;
	UA_Int32 packetSize;
	UA_Int32 sizePadding;
	UA_Int32 sizeSignature;


	sizeAsymAlgHeader = 3 * sizeof(UA_UInt32) +
			connection->secureLayer.localAsymAlgSettings.SecurityPolicyUri.length +
			connection->secureLayer.localAsymAlgSettings.SenderCertificate.length +
			connection->secureLayer.localAsymAlgSettings.ReceiverCertificateThumbprint.length;
	pos = 0;
	//sequence header
	sequenceNumber = connection->secureLayer.sequenceNumber;
    requestId = connection->secureLayer.requestId;


	if(type == 449) //openSecureChannelResponse -> asymmetric algorithm
	{
		//TODO fill with valid sizes
		sizePadding = 0;
		sizeSignature = 0;

		//TODO: size calculation need
		packetSize = SIZE_SECURECHANNEL_HEADER +
				SIZE_SEQHEADER_HEADER +
				sizeAsymAlgHeader +
				responseMessage.length +
				sizePadding +
				sizeSignature;

		//get memory for response
		UA_alloc((void**)&(responsePacket.data),packetSize);

		responsePacket.length = packetSize;

		/*---encode Secure Conversation Message Header ---*/
		//encode MessageType - OPN message
		responsePacket.data[0] = 'O';
		responsePacket.data[1] = 'P';
		responsePacket.data[2] = 'N';
		pos += 3;
		//encode Chunk Type - set to final
		responsePacket.data[3] = 'F';
		pos += 1;
		UA_Int32_encode(&packetSize,&pos,responsePacket.data);
		UA_UInt32_encode(&(connection->secureLayer.securityToken.secureChannelId),&pos,responsePacket.data);

		/*---encode Asymmetric Algorithm Header ---*/
		UA_ByteString_encode(&(connection->secureLayer.localAsymAlgSettings.SecurityPolicyUri),
						&pos,responsePacket.data);
		UA_ByteString_encode(&(connection->secureLayer.localAsymAlgSettings.SenderCertificate),
						&pos,responsePacket.data );
		UA_ByteString_encode(&(connection->secureLayer.localAsymAlgSettings.ReceiverCertificateThumbprint),
						&pos,responsePacket.data );
	}




	/*---encode Sequence Header ---*/
	UA_UInt32_encode(&sequenceNumber,&pos,responsePacket.data);
	UA_UInt32_encode(&requestId,&pos,responsePacket.data);

	/*---add encoded Message ---*/
	memcpy(&(responsePacket.data[pos]), responseMessage.data, responseMessage.length);

	/* sign Data*/

	/* encrypt Data */

	/* send Data */
	TL_send(connection,&responsePacket);


	return UA_NO_ERROR;
}

/*
 * opens a secure channel
 */
UA_Int32 SL_openSecureChannel(UA_connection *connection,
		UA_RequestHeader *requestHeader,
		UA_StatusCode serviceResult)
{



	UA_ResponseHeader responseHeader;
	UA_ExtensionObject additionalHeader;
	SL_ChannelSecurityToken securityToken;
	UA_ByteString serverNonce;
	UA_NodeId responseType;
	//sizes for memory allocation
	UA_Int32 sizeResponse;
	UA_Int32 sizeRespHeader;
	UA_Int32 sizeResponseType;
	UA_Int32 sizeRespMessage;
	UA_Int32 sizeSecurityToken;
	UA_ByteString response;
	UA_UInt32 serverProtocolVersion;
	UA_Int32 pos;
	UA_DiagnosticInfo serviceDiagnostics;

	if(requestHeader->returnDiagnostics != 0)
	{
		printf("SL_openSecureChannel - diagnostics demanded by the client\n");
		printf("SL_openSecureChannel - retrieving diagnostics not implemented!\n");
		//TODO fill with demanded information part 4, 7.8 - Table 123
		serviceDiagnostics.encodingMask = 0;
	}
	else
	{
		serviceDiagnostics.encodingMask = 0;
	}
	/*--------------type ----------------------*/

	//Four Bytes Encoding
	responseType.encodingByte = UA_NODEIDTYPE_FOURBYTE;
	//openSecureChannelResponse = 449
	responseType.identifier.numeric = 449;
	responseType.namespace = 0;

	/*--------------responseHeader-------------*/

	/* 	Res-1) ResponseHeader responseHeader
	 * 		timestamp UtcTime
	 * 		requestHandle IntegerId
	 * 		serviceResult StatusCode
	 * 		serviceDiagnostics DiagnosticInfo
	 * 		stringTable[] String
	 * 		addtionalHeader Extensible Parameter
	 */
	//current time

	responseHeader.timestamp = UA_DateTime_now();
	//request Handle which client sent
	responseHeader.requestHandle = requestHeader->requestHandle;
	// StatusCode which informs client about quality of response
	responseHeader.serviceResult = serviceResult;
	//retrieve diagnosticInfo if client demands
	responseHeader.serviceDiagnostics = &serviceDiagnostics;

	//text of fields defined in the serviceDiagnostics
	responseHeader.stringTableSize = 0;
	responseHeader.stringTable = NULL;


	// no additional header
	additionalHeader.encoding = 0;

	additionalHeader.body.data = NULL;
	additionalHeader.body.length = 0;

	additionalHeader.typeId.encodingByte = 0;
	additionalHeader.typeId.namespace = 0;
	additionalHeader.typeId.identifier.numeric = 0;

	responseHeader.additionalHeader = &additionalHeader;
	printf("SL_openSecureChannel - built response header\n");

	//calculate the size
	sizeRespHeader = UA_ResponseHeader_calcSize(&responseHeader);
	printf("SL_openSecureChannel - size response header =%d\n",sizeRespHeader);
	/*--------------responseMessage-------------*/
	/* 	Res-2) UInt32 ServerProtocolVersion
	 * 	Res-3) SecurityToken channelSecurityToken
	 *  Res-5) ByteString ServerNonce
	*/

	//                  secureChannelId + TokenId + CreatedAt + RevisedLifetime
	sizeSecurityToken = sizeof(UA_UInt32) + sizeof(UA_UInt32) + sizeof(UA_DateTime) + sizeof(UA_Int32);

	//ignore server nonce
	serverNonce.length = -1;
	serverNonce.data = NULL;

	serverNonce.length = connection->secureLayer.localNonce.length;
	serverNonce.data = connection->secureLayer.localNonce.data;

	//fill token structure with default server information
	securityToken.secureChannelId = connection->secureLayer.securityToken.secureChannelId;
	securityToken.tokenId = connection->secureLayer.securityToken.tokenId;
	securityToken.createdAt = UA_DateTime_now();
	securityToken.revisedLifetime = connection->secureLayer.securityToken.revisedLifetime;

	serverProtocolVersion = connection->transportLayer.localConf.protocolVersion;

	//                ProtocolVersion + SecurityToken + Nonce
	sizeRespMessage = sizeof(UA_UInt32) + serverNonce.length + sizeof(UA_Int32) + sizeSecurityToken;
	printf("SL_openSecureChannel - size of response message=%d\n",sizeRespMessage);


	//get memory for response
	sizeResponseType = UA_NodeId_calcSize(&responseType);

	response.length = sizeResponseType + sizeRespHeader + sizeRespMessage;

	//FIXME: Sten 4 bytes are missing
	response.length += 4;

	//get memory for response
	UA_alloc((void*)&(response.data), response.length);
	pos = 0;
	//encode responseType (NodeId)
	UA_NodeId_printf("SL_openSecureChannel - TypeId =",&responseType);

	UA_NodeId_encode(&responseType, &pos, response.data);

	//encode header
	printf("SL_openSecureChannel - encoding response header \n");

	UA_ResponseHeader_encode(&responseHeader, &pos, response.data);
	printf("SL_openSecureChannel - response header encoded \n");

	//encode message
	printf("SL_openSecureChannel - serverProtocolVersion = %d \n",serverProtocolVersion);
	UA_UInt32_encode(&serverProtocolVersion, &pos,response.data);
	printf("SL_openSecureChannel - secureChannelId = %d \n",securityToken.secureChannelId);
	UA_UInt32_encode(&(securityToken.secureChannelId), &pos,response.data);
	printf("SL_openSecureChannel - tokenId = %d \n",securityToken.tokenId);
	UA_UInt32_encode(&(securityToken.tokenId), &pos,response.data);

	UA_DateTime_encode(&(securityToken.createdAt), &pos,response.data);
	printf("SL_openSecureChannel - revisedLifetime = %d \n",securityToken.revisedLifetime);
	UA_Int32_encode(&(securityToken.revisedLifetime), &pos,response.data);

	UA_ByteString_encode(&serverNonce, &pos,response.data);

	printf("SL_openSecureChannel - response.length = %d \n",response.length);

	printf("-----> reserved: %i\n", response.length);
	printf("-----> encoded: %i\n", pos);
	//FIXME: Sten: here is a problem

	//449 = openSecureChannelResponse
	SL_send(connection, response, 449);

	UA_free(response.data);

	return UA_SUCCESS;
}
/*
Int32 SL_openSecureChannel_responseMessage_calcSize(SL_Response *response,
		Int32* sizeInOut) {
	Int32 length = 0;
	length += sizeof(response->SecurityToken);
	length += UAString_calcSize(response->ServerNonce);
	length += sizeof(response->ServerProtocolVersion);
	return length;
}
*/
/*
 * closes a secureChannel (server side)
 */
void SL_secureChannel_close(UA_connection *connection) {

}
UA_Int32 SL_check(UA_connection *connection, UA_ByteString secureChannelPacket) {
	return UA_NO_ERROR;
}
UA_Int32 SL_createSecurityToken(UA_connection *connection, UA_Int32 lifeTime) {
	return UA_NO_ERROR;
}

UA_Int32 SL_processMessage(UA_connection *connection, UA_ByteString message) {
	UA_DiagnosticInfo serviceDiagnostics;

	UA_Int32 pos = 0;
	UA_RequestHeader requestHeader;
	UA_UInt32 clientProtocolVersion;
	UA_NodeId serviceRequestType;
	UA_Int32 requestType;
	UA_Int32 securityMode;
	UA_Int32 requestedLifetime;
	UA_ByteString clientNonce;
	UA_StatusCode serviceResult;

	// Every Message starts with a NodeID which names the serviceRequestType
	UA_NodeId_decode(message.data, &pos,
			&serviceRequestType);
	UA_NodeId_printf("SL_processMessage - serviceRequestType=",
			&serviceRequestType);

	if (serviceRequestType.encodingByte == UA_NODEIDTYPE_FOURBYTE
			&& serviceRequestType.identifier.numeric == 446) {
		/* OpenSecureChannelService, defined in 62541-6 §6.4.4, Table 34.
		 * Note that part 6 adds ClientProtocolVersion and ServerProtocolVersion
		 * to the definition in part 4 */
		// 	Req-1) RequestHeader requestHeader
		UA_RequestHeader requestHeader;
		UA_ExtensionObject additionalHeader;
		//FIXME: Sten the structure was empty and decode attempted to fill it -> segfault
		//FIXME: Sten: probably not needed anymore, since memory gets malloced in the decoder
		requestHeader.additionalHeader = &additionalHeader;
		// 	Req-2) UInt32 ClientProtocolVersion
		UA_UInt32 clientProtocolVersion;
		// 	Req-3) Enum SecurityTokenRequestType requestType
		UA_Int32 requestType;
		// 	Req-4) Enum MessageSecurityMode SecurityMode
		UA_Int32 securityMode;
		//  Req-5) ByteString ClientNonce
		UA_ByteString clientNonce = {0, NULL};
		//  Req-6) Int32 RequestLifetime
		UA_Int32 requestedLifetime;

		UA_ByteString_printx("SL_processMessage - message=", &message);
		// Req-1) RequestHeader requestHeader
		UA_RequestHeader_decode(message.data, &pos, &requestHeader);
		UA_String_printf("SL_processMessage - requestHeader.auditEntryId=",
				&(requestHeader.auditEntryId));
		UA_NodeId_printf(
				"SL_processMessage - requestHeader.authenticationToken=",
				&(requestHeader.authenticationToken));

		// 	Req-2) UInt32 ClientProtocolVersion
		UA_UInt32_decode(message.data, &pos,
				&clientProtocolVersion);
		printf("SL_processMessage - clientProtocolVersion=%d\n",
				clientProtocolVersion);

		if (clientProtocolVersion
				!= connection->transportLayer.remoteConf.protocolVersion) {
			printf("SL_processMessage - error protocol version \n");
			//TODO error protocol version
			//TODO ERROR_Bad_ProtocolVersionUnsupported

		}

		// 	Req-3) SecurityTokenRequestType requestType
		UA_Int32_decode(message.data, &pos, &requestType);
		printf("SL_processMessage - requestType=%d\n", requestType);
		switch (requestType) {
		case UA_SECURITYTOKEN_ISSUE:
			if (connection->secureLayer.connectionState
					== connectionState_ESTABLISHED) {
				printf("SL_processMessage - multiply security token request");
				//TODO return ERROR
				return UA_ERROR;
			}
			printf(
					"SL_processMessage - TODO: create new token for a new SecureChannel\n");
			//	SL_createNewToken(connection);
			break;
		case UA_SECURITYTOKEN_RENEW:
			if (connection->secureLayer.connectionState
					== connectionState_CLOSED) {
				printf(
						"SL_processMessage - renew token request received, but no secureChannel was established before");
				//TODO return ERROR
				return UA_ERROR;
			}
			printf("TODO: create new token for an existing SecureChannel\n");
			break;
		}

		// 	Req-4) MessageSecurityMode SecurityMode
		UA_Int32_decode(message.data, &pos, &securityMode);
		printf("SL_processMessage - securityMode=%d\n", securityMode);
		switch (securityMode) {
		case UA_SECURITYMODE_INVALID:
			connection->secureLayer.remoteNonce.data = NULL;
			connection->secureLayer.remoteNonce.length = 0;
			printf("SL_processMessage - client demands no security \n");
			break;

		case UA_SECURITYMODE_SIGN:
			printf("SL_processMessage - client demands signed \n");
			//TODO check if senderCertificate and ReceiverCertificateThumbprint are present
			break;

		case UA_SECURITYMODE_SIGNANDENCRYPT:
			printf("SL_processMessage - client demands signed & encrypted \n");
			//TODO check if senderCertificate and ReceiverCertificateThumbprint are present
			break;
		}

		//  Req-5) ByteString ClientNonce
		UA_ByteString_decode(message.data, &pos,
				&clientNonce);
		UA_ByteString_printf("SL_processMessage - clientNonce=", &clientNonce);

		//deleteMembers is important since clientNonce is on stack
		UA_ByteString_deleteMembers(&clientNonce);

		//  Req-6) Int32 RequestLifetime
		UA_Int32_decode(message.data, &pos,
				&requestedLifetime);
		printf("SL_processMessage - requestedLifeTime=%d\n", requestedLifetime);
		//TODO process requestedLifetime

		// 62541-4 §7.27 "The requestHandle given by the Client to the request."
		return SL_openSecureChannel(connection, &requestHeader, SC_Good);
	} else {
		printf("SL_processMessage - unknown service request");
		//TODO change error code
		return UA_ERROR;

	}
	return UA_SUCCESS;
}
/*
 * receive and process data from underlying layer
 */
void SL_receive(UA_connection *connection, UA_ByteString *serviceMessage) {
	UA_ByteString secureChannelPacket;
	UA_ByteString message;
	UA_SecureConversationMessageHeader secureConvHeader;
	UA_AsymmetricAlgorithmSecurityHeader asymAlgSecHeader;
	UA_SequenceHeader sequenceHeader;
	// UA_Int32 packetType = 0;
	UA_Int32 pos = 0;
	UA_Int32 iTmp;
	//TODO Error Handling, length checking
	//get data from transport layer
	printf("SL_receive - entered \n");

	TL_receive(connection, &secureChannelPacket);

	if (secureChannelPacket.length > 0 && secureChannelPacket.data != NULL) {

		printf("SL_receive - data received \n");
		//packetType = TL_getPacketType(&secureChannelPacket, &pos);

		UA_SecureConversationMessageHeader_decode(secureChannelPacket.data, &pos, &secureConvHeader);

		switch (secureConvHeader.tcpMessageHeader->messageType) {

		case UA_MESSAGETYPE_OPN: /* openSecureChannel Message received */
			UA_AsymmetricAlgorithmSecurityHeader_decode(secureChannelPacket.data, &pos, &asymAlgSecHeader);
			UA_ByteString_printf("SL_receive - AAS_Header.ReceiverThumbprint=",
					&(asymAlgSecHeader.receiverCertificateThumbprint));
			UA_ByteString_printf("SL_receive - AAS_Header.SecurityPolicyUri=",
					&(asymAlgSecHeader.securityPolicyUri));
			UA_ByteString_printf("SL_receive - AAS_Header.SenderCertificate=",
					&(asymAlgSecHeader.senderCertificate));
			if (secureConvHeader.secureChannelId != 0) {

				iTmp = UA_ByteString_compare(
						&(connection->secureLayer.remoteAsymAlgSettings.SenderCertificate),
						&(asymAlgSecHeader.senderCertificate));
				if (iTmp != UA_EQUAL) {
					printf("SL_receive - UA_ERROR_BadSecureChannelUnknown \n");
					//TODO return UA_ERROR_BadSecureChannelUnknown
				}

			//FIXME: destroy decodeAASHeader (to prevent memleak)
			// UA_AsymmetricAlgorithmSecurityHeader_deleteMembers(&asymAlgSecHeader);
			}
			else
			{
				//TODO invalid securechannelId
			}

			UA_SequenceHeader_decode(secureChannelPacket.data, &pos, &sequenceHeader);
			printf("SL_receive - SequenceHeader.RequestId=%d\n",
					sequenceHeader.requestId);
			printf("SL_receive - SequenceHeader.SequenceNr=%d\n",
					sequenceHeader.sequenceNumber);
			//save request id to return it to client
			connection->secureLayer.requestId = sequenceHeader.requestId;
			//TODO check that the sequence number is smaller than MaxUInt32 - 1024
			connection->secureLayer.sequenceNumber =
					sequenceHeader.sequenceNumber;

			//SL_decrypt(&secureChannelPacket);
			message.data = &secureChannelPacket.data[pos];
			message.length = secureChannelPacket.length - pos;
			UA_ByteString_printx("SL_receive - message=",&message);

			SL_processMessage(connection, message);

			break;
		case UA_MESSAGETYPE_MSG: /* secure Channel Message received */
			if (connection->secureLayer.connectionState
					== connectionState_ESTABLISHED) {

				if (secureConvHeader.secureChannelId
						== connection->secureLayer.securityToken.secureChannelId) {

				} else {
					//TODO generate ERROR_Bad_SecureChannelUnkown
				}
			}

			break;
		case UA_MESSAGETYPE_CLO: /* closeSecureChannel Message received */
			if (SL_check(connection, secureChannelPacket) == UA_NO_ERROR) {

			}
			break;
		}

	} else {
		printf("SL_receive - no data received \n");
	}
	/*
	 Int32 readPosition = 0;

	 //get the Secure Channel Message Header
	 decodeSCMHeader(secureChannelPacket,
	 &readPosition, &SCM_Header);

	 //get the Secure Channel Asymmetric Algorithm Security Header
	 decodeAASHeader(secureChannelPacket,
	 &readPosition, &AAS_Header);

	 //get the Sequence Header
	 decodeSequenceHeader(secureChannelPacket,
	 &readPosition, &SequenceHeader);

	 //get Secure Channel Message
	 //SL_secureChannel_Message_get(connection, secureChannelPacket,
	 //			&readPosition,serviceMessage);

	 if (secureChannelPacket->length > 0)
	 {
	 switch (SCM_Header.MessageType)
	 {
	 case packetType_MSG:
	 if (connection->secureLayer.connectionState
	 == connectionState_ESTABLISHED)
	 {

	 }
	 else //receiving message, without secure channel
	 {
	 //TODO send back Error Message
	 }
	 break;
	 case packetType_OPN:
	 //Server Handling
	 //		if (openSecureChannelHeader_check(connection, secureChannelPacket))
	 //		{
	 //check if the request is valid
	 //	SL_openSecureChannelRequest_check(connection, secureChannelPacket);
	 //		}
	 //		else
	 //		{
	 //			//TODO send back Error Message
	 //		}
	 //Client Handling

	 //TODO free memory for secureChannelPacket

	 break;
	 case packetType_CLO:


	 //TODO free memory for secureChannelPacket
	 break;
	 }

	 }
	 */
}


