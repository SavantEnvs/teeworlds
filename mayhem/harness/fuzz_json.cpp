// JSON parser used for server-list/config data, followed by a full walk of the parsed tree.
#include <engine/shared/jsonparser.h>

#include <stdint.h>
#include <string.h>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *pData, size_t Size)
{
	// Every caller parses a file through CJsonParser::ParseFile, whose IStorage::ReadFile/io_read_all
	// buffer always ends in a NUL after the data (base/system.c); hand the parser the same layout.
	std::vector<char> Buf(pData, pData + Size);
	Buf.push_back('\0');
	CJsonParser Parser;
	const json_value *pRoot = Parser.ParseData(Buf.data(), (unsigned)Size);
	if(!pRoot)
		return -1;

	volatile unsigned Sink = 0;
	std::vector<const json_value *> Stack;
	Stack.push_back(pRoot);
	while(!Stack.empty())
	{
		const json_value *pValue = Stack.back();
		Stack.pop_back();
		switch(pValue->type)
		{
		case json_object:
			for(unsigned i = 0; i < pValue->u.object.length; i++)
			{
				Sink = Sink + (unsigned)strlen(pValue->u.object.values[i].name);
				Stack.push_back(pValue->u.object.values[i].value);
			}
			break;
		case json_array:
			for(unsigned i = 0; i < pValue->u.array.length; i++)
				Stack.push_back(pValue->u.array.values[i]);
			break;
		case json_string:
			Sink = Sink + (unsigned)strlen(pValue->u.string.ptr);
			break;
		case json_integer:
			Sink = Sink + (unsigned)pValue->u.integer;
			break;
		case json_double:
			Sink = Sink + (unsigned)(pValue->u.dbl != 0.0);
			break;
		case json_boolean:
			Sink = Sink + (unsigned)pValue->u.boolean;
			break;
		default:
			break;
		}
	}
	return 0;
}
