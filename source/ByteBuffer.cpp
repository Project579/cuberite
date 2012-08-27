
// ByteBuffer.cpp

// Implements the cByteBuffer class representing a ringbuffer of bytes

#include "Globals.h"

#include "ByteBuffer.h"
#include "Endianness.h"





#define NEEDBYTES(Num) if (!CanReadBytes(Num)) return false;





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// cByteBuffer:

cByteBuffer::cByteBuffer(int a_BufferSize) :
	m_Buffer(new char[a_BufferSize + 1]),
	m_BufferSize(a_BufferSize + 1),
	m_DataStart(0),
	m_WritePos(0),
	m_ReadPos(0)
{
	// Allocating one byte more than the buffer size requested, so that we can distinguish between
	// completely-full and completely-empty states
}





cByteBuffer::~cByteBuffer()
{
	delete m_Buffer;
}





bool cByteBuffer::Write(const char * a_Bytes, int a_Count)
{
	if (GetFreeSpace() < a_Count)
	{
		return false;
	}
	int TillEnd = m_BufferSize - m_WritePos;
	if (TillEnd < a_Count)
	{
		// Need to wrap around the ringbuffer end
		memcpy(m_Buffer + m_WritePos, a_Bytes, TillEnd);
		m_WritePos = 0;
		a_Bytes += TillEnd;
		a_Count -= TillEnd;
	}
	
	// We're guaranteed that we'll fit in a single write op
	memcpy(m_Buffer + m_WritePos, a_Bytes, a_Count);
	m_WritePos += a_Count;
	return true;
}





int cByteBuffer::GetFreeSpace(void) const
{
	if (m_WritePos >= m_DataStart)
	{
		// Wrap around the buffer end:
		return m_BufferSize - m_WritePos + m_DataStart - 1;
	}
	// Single free space partition:
	return m_DataStart - m_WritePos - 1;
}





/// Returns the number of bytes that are currently in the ringbuffer. Note GetReadableBytes()
int cByteBuffer::GetUsedSpace(void) const
{
	return m_BufferSize - GetFreeSpace();
}





/// Returns the number of bytes that are currently available for reading (may be less than UsedSpace due to some data having been read already)
int cByteBuffer::GetReadableSpace(void) const
{
	if (m_ReadPos > m_WritePos)
	{
		// Wrap around the buffer end:
		return m_BufferSize - m_ReadPos + m_WritePos;
	}
	// Single readable space partition:
	return m_WritePos - m_ReadPos ;
}





bool cByteBuffer::CanReadBytes(int a_Count) const
{
	return (a_Count <= GetReadableSpace());
}





bool cByteBuffer::ReadChar(char & a_Value)
{
	NEEDBYTES(1);
	a_Value = m_Buffer[m_ReadPos++];
	return true;
}





bool cByteBuffer::ReadByte(unsigned char & a_Value)
{
	NEEDBYTES(1);
	a_Value = (unsigned char)(m_Buffer[m_ReadPos++]);
	return true;
}





bool cByteBuffer::ReadBEShort(short & a_Value)
{
	NEEDBYTES(2);
	ReadBuf(&a_Value, 2);
	a_Value = ntohs(a_Value);
	return true;
}





bool cByteBuffer::ReadBEInt(int & a_Value)
{
	NEEDBYTES(4);
	ReadBuf(&a_Value, 4);
	a_Value = ntohl(a_Value);
	return true;
}





bool cByteBuffer::ReadBEInt64(Int64 & a_Value)
{
	NEEDBYTES(8);
	ReadBuf(&a_Value, 8);
	a_Value = NetworkToHostLong8(&a_Value);
	return true;
}





bool cByteBuffer::ReadBEFloat(float & a_Value)
{
	NEEDBYTES(4);
	ReadBuf(&a_Value, 4);
	a_Value = NetworkToHostFloat4(&a_Value);
	return true;
}





bool cByteBuffer::ReadBEDouble(double & a_Value)
{
	NEEDBYTES(8);
	ReadBuf(&a_Value, 8);
	a_Value = NetworkToHostDouble8(&a_Value);
	return true;
}





bool cByteBuffer::ReadBool(bool & a_Value)
{
	NEEDBYTES(1);
	a_Value = (m_Buffer[m_ReadPos++] != 0);
	return true;
}





bool cByteBuffer::ReadBEUTF16String16(AString & a_Value)
{
	short Length;
	if (!ReadBEShort(Length))
	{
		return false;
	}
	return ReadUTF16String(a_Value, Length);
}





bool cByteBuffer::ReadBuf(void * a_Buffer, int a_Count)
{
	NEEDBYTES(a_Count);
	char * Dst = (char *)a_Buffer;  // So that we can do byte math
	int BytesToEndOfBuffer = m_BufferSize - m_ReadPos;
	if (BytesToEndOfBuffer < a_Count)
	{
		// Reading across the ringbuffer end, read the first part and adjust parameters:
		memcpy(Dst, m_Buffer + m_ReadPos, BytesToEndOfBuffer);
		Dst += BytesToEndOfBuffer;
		a_Count -= BytesToEndOfBuffer;
		m_ReadPos = 0;
	}
	
	// Read the rest of the bytes in a single read (guaranteed to fit):
	memcpy(Dst, m_Buffer + m_ReadPos, a_Count);
	m_ReadPos += a_Count;
	return true;
}





bool cByteBuffer::ReadString(AString & a_String, int a_Count)
{
	NEEDBYTES(a_Count);
	a_String.clear();
	a_String.reserve(a_Count);
	int BytesToEndOfBuffer = m_BufferSize - m_ReadPos;
	if (BytesToEndOfBuffer < a_Count)
	{
		// Reading across the ringbuffer end, read the first part and adjust parameters:
		a_String.assign(m_Buffer + m_ReadPos, BytesToEndOfBuffer);
		a_Count -= BytesToEndOfBuffer;
		m_ReadPos = 0;
	}
	
	// Read the rest of the bytes in a single read (guaranteed to fit):
	a_String.append(m_Buffer + m_ReadPos, a_Count);
	m_ReadPos += a_Count;
	return true;
}





bool cByteBuffer::ReadUTF16String(AString & a_String, int a_NumChars)
{
	// Reads 2 * a_NumChars bytes and interprets it as a UTF16 string, converting it into UTF8 string a_String
	AString RawData;
	if (!ReadString(RawData, a_NumChars * 2))
	{
		return false;
	}
	RawBEToUTF8((short *)(RawData.data()), a_NumChars, a_String);
	return true;
}





bool cByteBuffer::SkipRead(int a_Count)
{
	if (!CanReadBytes(a_Count))
	{
		return false;
	}
	AdvanceReadPos(a_Count);
	return true;
}





void cByteBuffer::ReadAll(AString & a_Data)
{
	ReadString(a_Data, GetReadableSpace());
}





void cByteBuffer::CommitRead(void)
{
	m_DataStart = m_ReadPos;
}





void cByteBuffer::ResetRead(void)
{
	m_ReadPos = m_DataStart;
}





void cByteBuffer::AdvanceReadPos(int a_Count)
{
	m_ReadPos += a_Count;
	if (m_ReadPos > m_BufferSize)
	{
		m_ReadPos -= m_BufferSize;
	}
}




