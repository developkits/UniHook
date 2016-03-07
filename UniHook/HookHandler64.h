#pragma once
typedef void(__stdcall* tGeneric)();
__declspec(noinline) void Interupt1()
{
	XTrace("[+] In Interupt1\n");
}

__declspec(noinline) void Interupt2()
{
	XTrace("[+] In Interupt2\n");
}

BYTE ABS_JMP_ASM[] = { 0x50, 0x48, 0xB8, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x48, 0x87, 0x04, 0x24, 0xC3 };
volatile int WriteAbsoluteJMP(BYTE* Destination, DWORD64 JMPDestination)
{
	/*push rax
	mov rax ...   //Address to original
	xchg qword ptr ss:[rsp], rax
	ret*/
	memcpy(Destination, ABS_JMP_ASM, sizeof(ABS_JMP_ASM));
	*(DWORD64*)&((BYTE*)Destination)[3] = JMPDestination;
	return sizeof(ABS_JMP_ASM);
}

volatile int WriteAbsoluteCall(BYTE* Destination, DWORD64 JMPDestination)
{
	/*
	mov rax, ...
	call rax
	*/
	BYTE call[] = {0x48, 0xB8, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xFF, 0xD0 };
	memcpy(Destination, call, sizeof(call));
	*(DWORD64*)&((BYTE*)Destination)[2] = JMPDestination;
	return sizeof(call);
}

volatile int WritePUSHA(BYTE* Address)
{
	/*

	*/
	BYTE X64PUSHFA[] = { 0x53, 0x54, 0x50, 0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, 0x48, 0x83, 0xEC, 0x20 };
	memcpy(Address, X64PUSHFA, sizeof(X64PUSHFA));
	return sizeof(X64PUSHFA);
}

volatile int WritePOPA(BYTE* Address)
{
	/*
	
	*/
	BYTE X64POPFA[] = { 0x48, 0x83, 0xC4, 0x20, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59, 0x58, 0x5C, 0x5B };
	memcpy(Address, X64POPFA, sizeof(X64POPFA));
	return sizeof(X64POPFA);
}

#define LODWORD(_qw)    ((DWORD)(_qw))
#define HIDWORD(_qw)    ((DWORD)(((_qw) >> 32) & 0xffffffff))
BYTE PushRet[] = { 0x48, 0x83, 0xEC, 0x08, 0xC7, 0x04, 0x24, 0xCC, 0xCC, 0xCC, 0xCC, 0xC7, 0x44, 0x24, 0x04, 0xCC, 0xCC, 0xCC, 0xCC };
volatile int WriteRetAddress(BYTE* Address, DWORD64 Destination)
{
	/*
	sub rsp,8
	mov dword ptr [rsp],0xCCCCCCCC
	mov dword ptr [rsp + 0x4],0xCCCCCCCC
	*/
	memcpy(Address, PushRet, sizeof(PushRet));
	*(DWORD*)(Address + 7) = LODWORD(Destination);
	*(DWORD*)(Address + 15) = HIDWORD(Destination);
	return sizeof(PushRet);
}

BYTE SHADOWSUB_ASM[] = { 0x48, 0x83, 0xEC, 0x28 };
volatile int WriteSubShadowSpace(BYTE* Address)
{
	memcpy(Address, SHADOWSUB_ASM, sizeof(SHADOWSUB_ASM));
	return sizeof(SHADOWSUB_ASM);
}

BYTE SHADOWADD_ASM[] = { 0x48, 0x83, 0xC4, 0x28 };
volatile int WriteAddShadowSpace(BYTE* Address)
{
	memcpy(Address, SHADOWADD_ASM, sizeof(SHADOWADD_ASM));
	return sizeof(SHADOWADD_ASM);
}

volatile int WriteRET(BYTE* Address)
{
	BYTE ret[] = { 0xC3 };
	memcpy(Address, ret, sizeof(ret));
	return sizeof(ret);
}

void HookFunctionAtRuntime(BYTE* SubRoutineAddress, HookMethod Method)
{
	BYTE* Callback = new BYTE[1024];
	DWORD Old;
	VirtualProtect(Callback, 1024, PAGE_EXECUTE_READWRITE, &Old);

	std::shared_ptr<PLH::IHook> Hook;
	DWORD64 Original;
	if (Method == HookMethod::INLINE)
	{
		Hook.reset(new PLH::Detour, [](PLH::Detour* Hook) {
			Hook->UnHook();
			delete Hook;
		});
		((PLH::Detour*)Hook.get())->SetupHook((BYTE*)SubRoutineAddress, (BYTE*)Callback);
		Hook->Hook();
		Original = (DWORD64)((PLH::Detour*)Hook.get())->GetOriginal();
	}
	else if (Method == HookMethod::INT3_BP) {
		Hook.reset(new PLH::VEHHook, [](PLH::VEHHook* Hook) {
			Hook->UnHook();
			delete Hook;
		});
		((PLH::VEHHook*)Hook.get())->SetupHook((BYTE*)SubRoutineAddress, (BYTE*)Callback, PLH::VEHHook::VEHMethod::INT3_BP);
		Hook->Hook();
		Original = (DWORD64)((PLH::VEHHook*)Hook.get())->GetOriginal();
	}

	int WriteOffset = 0;
	WriteOffset += WritePUSHA(Callback);
	WriteOffset += WriteAbsoluteCall(Callback + WriteOffset, (DWORD64)&Interupt1);
	WriteOffset += WritePOPA(Callback + WriteOffset);

	WriteOffset += WriteSubShadowSpace(Callback+WriteOffset);
	WriteOffset += WriteRetAddress(Callback + WriteOffset,(DWORD64)(Callback+WriteOffset+sizeof(PushRet) +sizeof(ABS_JMP_ASM)));
	WriteOffset += WriteAbsoluteJMP(Callback + WriteOffset, Original);
	WriteOffset += WriteAddShadowSpace(Callback + WriteOffset);

	WriteOffset += WritePUSHA(Callback + WriteOffset);
	WriteOffset += WriteAbsoluteCall(Callback + WriteOffset, (DWORD64)&Interupt2);
	WriteOffset += WritePOPA(Callback + WriteOffset);

	WriteOffset += WriteRET(Callback + WriteOffset);

	XTrace("[+] Callback at: %I64X\n", Callback);
	m_Hooks.push_back(Hook);
	m_Callbacks.push_back(Callback);
}