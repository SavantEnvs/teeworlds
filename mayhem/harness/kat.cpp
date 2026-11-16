// Known-answer probe: runs fixed inputs through the real codecs/parsers and prints the results, which
// mayhem/test.sh compares against golden values.
#include <engine/console.h>
#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/shared/huffman.h>
#include <engine/shared/jsonparser.h>
#include <engine/shared/packer.h>

#include <stdio.h>
#include <string.h>

static void Hex(const char *pLabel, const unsigned char *pData, int Size)
{
	printf("%s", pLabel);
	for(int i = 0; i < Size; i++)
		printf(" %02x", pData[i]);
	printf("\n");
}

static void ProbeCallback(IConsole::IResult *pResult, void *pUser)
{
	printf("console probe: n=%d i=%d f=%.2f s=%s r=%s\n", pResult->NumArguments(), pResult->GetInteger(0),
		pResult->GetFloat(1), pResult->GetString(2), pResult->GetString(3));
}

int main()
{
	unsigned char aBuf[64];

	unsigned char *pEnd = CVariableInt::Pack(aBuf, 1000, sizeof(aBuf));
	Hex("varint 1000:", aBuf, (int)(pEnd - aBuf));
	pEnd = CVariableInt::Pack(aBuf, -1, sizeof(aBuf));
	Hex("varint -1:", aBuf, (int)(pEnd - aBuf));
	const int aInts[4] = {0, 63, -64, 123456};
	const long Packed = CVariableInt::Compress(aInts, sizeof(aInts), aBuf, sizeof(aBuf));
	Hex("varint block:", aBuf, (int)Packed);
	int aBack[4];
	printf("varint back: %ld %d %d %d %d\n", CVariableInt::Decompress(aBuf, (int)Packed, aBack, sizeof(aBack)), aBack[0], aBack[1], aBack[2], aBack[3]);

	CHuffman Huffman;
	Huffman.Init();
	const char *pText = "teeworlds";
	const int HuffSize = Huffman.Compress(pText, (int)strlen(pText), aBuf, sizeof(aBuf));
	Hex("huffman teeworlds:", aBuf, HuffSize);
	char aText[32] = {0};
	const int TextSize = Huffman.Decompress(aBuf, HuffSize, aText, sizeof(aText));
	printf("huffman back: %d %s\n", TextSize, aText);

	CPacker Packer;
	Packer.Reset();
	Packer.AddInt(-5);
	Packer.AddInt(300);
	Packer.AddString("hi", 0);
	Hex("packer:", Packer.Data(), Packer.Size());
	CUnpacker Unpacker;
	Unpacker.Reset(Packer.Data(), Packer.Size());
	const int A = Unpacker.GetInt();
	const int B = Unpacker.GetInt();
	const char *pStr = Unpacker.GetString();
	printf("unpacker: %d %d %s err=%d\n", A, B, pStr, Unpacker.Error());

	const char *pJson = "{\"a\":[1,2,3],\"b\":\"x\",\"c\":{\"d\":true}}";
	CJsonParser Parser;
	const json_value *pRoot = Parser.ParseString(pJson);
	if(pRoot && pRoot->type == json_object)
	{
		const json_value &Root = *pRoot;
		printf("json: a1=%d b=%s d=%d len=%u\n", (int)(json_int_t)Root["a"][1], (const char *)Root["b"], (int)(bool)Root["c"]["d"], Root.u.object.length);
	}
	else
		printf("json: parse failed: %s\n", Parser.Error());

	IConsole *pConsole = CreateConsole(CFGFLAG_SERVER);
	pConsole->Register("probe", "i[a] i[b] s[c] r[d]", CFGFLAG_SERVER, ProbeCallback, 0, "probe");
	pConsole->ExecuteLine("probe 42 7 abc \"hello world\" tail");
	delete pConsole;
	return 0;
}
