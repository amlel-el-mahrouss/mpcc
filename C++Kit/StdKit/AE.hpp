/*
 * ========================================================
 *
 *      C++Kit
 *      Copyright Amlal El Mahrouss, all rights reserved.
 *
 * ========================================================
 */

#pragma once

#include <C++Kit/Defines.hpp>

#define kAEMag0 'A'
#define kAEMag1 'E'

#define kAESymbolLen 64
#define kAEPad    8
#define kAEMagLen 2
#define kAEInvalidOpcode 0x00

// Advanced Executable File Format for MetroLink.
// Reloctable by offset is the default strategy.
// You can also relocate at runtime but that's up to the operating system loader.

namespace CxxKit
{
	// @brief Advanced Executable Header
	// One thing to keep in mind.
	// This object format, is reloctable.
	typedef struct AEHeader final
	{
		CharType fMagic[kAEMagLen];
		CharType fArch;
		SizeType fCount;
		CharType fSize;
        SizeType fStartCode;
        SizeType fCodeSize;
		CharType fPad[kAEPad];
	} __attribute__((packed)) AEHeader, *AEHeaderPtr;

	// @brief Advanced Executable Record.
	// Could be data, code or bss.
	// fKind must be filled with PEF fields.

	typedef struct AERecordHeader final
	{
		CharType fName[kAESymbolLen];
        SizeType fKind;
		SizeType fSize;
		SizeType fFlags;
		UIntPtr fOffset;
        CharType fPad[kAEPad];
	} __attribute__((packed)) AERecordHeader, *AERecordHeaderPtr;

	enum
	{
		kKindRelocationByOffset = 0x23f,
		kKindRelocationAtRuntime = 0x34f,
	};
}
