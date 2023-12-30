/*
 *	========================================================
 *
 *	C++Kit
 * 	Copyright Amlal El Mahrouss, all rights reserved.
 *
 * 	========================================================
 */

#pragma once

#include <C++Kit/Defines.hpp>

#define kPefMagic    "PEF"
#define kPefMagicFat "FEP"

#define kPefMagicLen 3

#define kPefVersion 1
#define kPefNameLen 64

// Protable Executable Format, a format designed for any computer.

namespace CxxKit
{
    enum
    {
        kPefArchIntel86S = 100,
        kPefArchAMD64,
        kPefArchRISCV,
	    kPefArchARC, /* Advanced RISC architecture. */
        kPefArchNewCPU = kPefArchARC,
        kPefArchPowerPC,
        kPefArchARM64,
        kPefArchInvalid = 0xFF,
    };

    enum
    {
        kPefKindExec = 1, /* .o/.pef/<none>  */
        kPefKindSharedObject = 2, /* .lib */
        kPefKindObject = 4, /* .obj */
        kPefKindDwarf = 5, /* .dsym */
    };

    /* PEF container */
    typedef struct PEFContainer final
    {
        CharType Magic[kPefMagicLen];
        UInt32 Linker;
        UInt32 Version;
        UInt32 Kind;
        UInt32 Abi;
        UInt32 Cpu;
        UInt32 SubCpu; /* Cpu specific information */
        UIntPtr Start;
        SizeType HdrSz; /* Size of header */
        SizeType Count; /* container header count */
    } __attribute__((packed)) PEFContainer;

    /* First PEFCommandHeader starts after PEFContainer */
    /* Last container is __exec_end */

    /* PEF executable section and commands. */

    typedef struct PEFCommandHeader final
    {
        CharType Name[kPefNameLen]; /* container name */
        UInt32 Flags; /* container flags */
        UInt16 Kind; /* container kind */
        UIntPtr Offset; /* file offset */
        SizeType Size; /* file size */
    } __attribute__((packed)) PEFCommandHeader;

    enum
    {
        kPefCode = 0xC,
        kPefData = 0xD,
        kPefZero = 0xE,
	    kPefLinkerID = 0x1,
    };
}

#define kPefExt ".o"
#define kPefDylibExt ".so"
#define kPefObjectExt ".o"
#define kPefDebugExt ".dbg"
#define kPefOrgMagic 0xFFAABBAA00