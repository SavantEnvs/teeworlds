// Console command-line parser/interpreter: every input line is executed on a fresh console that only has
// the built-in commands plus a few argument-type probes, so no state survives between inputs.
#include <engine/console.h>
#include <engine/shared/config.h>

#include <stdint.h>
#include <string.h>
#include <string>

static volatile unsigned s_Sink;

static void ProbeCallback(IConsole::IResult *pResult, void *pUser)
{
	unsigned Sum = 0;
	for(int i = 0; i < pResult->NumArguments(); i++)
	{
		Sum += (unsigned)pResult->GetInteger(i);
		Sum += (unsigned)(pResult->GetFloat(i) > 0.0f);
		Sum += (unsigned)strlen(pResult->GetString(i));
	}
	s_Sink = s_Sink + Sum;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *pData, size_t Size)
{
	if(Size > 4096)
		return -1;
	std::string Line((const char *)pData, Size);

	IConsole *pConsole = CreateConsole(CFGFLAG_SERVER | CFGFLAG_CLIENT);
	pConsole->Register("probe_i", "i[a] ?i[b]", CFGFLAG_SERVER, ProbeCallback, 0, "int args");
	pConsole->Register("probe_f", "i[a] s[b] ?s[c]", CFGFLAG_SERVER, ProbeCallback, 0, "mixed args");
	pConsole->Register("probe_r", "s[a] r[rest]", CFGFLAG_SERVER, ProbeCallback, 0, "rest of line");
	pConsole->Register("probe_n", "", CFGFLAG_SERVER, ProbeCallback, 0, "no args");
	pConsole->Register("+probe_k", "", CFGFLAG_CLIENT, ProbeCallback, 0, "stroke command");

	pConsole->LineIsValid(Line.c_str());
	pConsole->ExecuteLine(Line.c_str());
	pConsole->ExecuteLineStroked(1, Line.c_str());
	pConsole->ExecuteLineFlag(Line.c_str(), CFGFLAG_SERVER);
	pConsole->PossibleCommands(Line.c_str(), CFGFLAG_SERVER | CFGFLAG_CLIENT, false);
	delete pConsole;
	return 0;
}
