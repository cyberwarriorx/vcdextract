/*  Copyright 2013-2016 Theo Berkau

    This file is part of VCDEXTRACT.

    VCDEXTRACT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    VCDEXTRACT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with VCDEXTRACT; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef ISO_H
#define ISO_H

typedef struct
{
    unsigned char Year;     // Number of years since 1900
    unsigned char Month;    // Month of the year from 1 to 12
    unsigned char Day;      // Day of the Month from 1 to 31
    unsigned char Hour;     // Hour of the day from 0 to 23
    unsigned char Minute;   // Minute of the hour from 0 to 59
    unsigned char Second;   // second of the minute from 0 to 59
             char Zone;     // Offset from Greenwich Mean Time in
                            // number of 15 minute intervals from
                            // -48(West) to +52(East)
} volumedatetime_struct;

#define ISOATTR_HIDDEN          0x0001
#define ISOATTR_DIRECTORY       0x0002
#define ISOATTR_ASSOCIATED      0x0004
#define ISOATTR_RECORD          0x0008
#define ISOATTR_PROTECTION      0x0010
#define ISOATTR_MULTIEXTENT     0x0080

#define XAATTR_FORM1            0x0800
#define XAATTR_FORM2            0x1000
#define XAATTR_INTERLEAVED      0x2000
#define XAATTR_CDDA             0x4000
#define XAATTR_DIR              0x8000

typedef struct
{
   unsigned short group_id;
   unsigned short user_id;
   unsigned short attributes;
   unsigned short signature;
   unsigned char file_number;
   unsigned char reserved[5];
} xadirrec_struct;

// xar notes:
// attributes:
// 0x4111 used for file names linked to cd audio tracks
// 

// Subheader notes
// Offset Contents
// 0      File number(when interleaving, 0 otherwise)
// 1      Channel number(for audio data, 0 if not used)
// 2      XA Flags(see below)
// 3      Coding flag(for audio or video, 0 otherwise)

// XA Flags
#define XAFLAG_EOR      0x01 // When using interleaving, it's the last sector
#define XAFLAG_VIDEO    0x02 // Video sector
#define XAFLAG_AUDIO    0x04 // Audio sector(encoded with ADPCM)
#define XAFLAG_DATA     0x08 // Data sector
#define XAFLAG_TRIGGER  0x10 // Not used on saturn(that I know of)
#define XAFLAG_FORM2    0x20 // Form 2 flag. If 0, Form 1 is used
#define XAFLAG_REALTIME 0x40 // Sector with realtime data
#define XAFLAG_EOF      0x80 // End of File

typedef struct dirrec_struct
{
    unsigned char LengthOfDirectoryRecord;  //
    unsigned char ExtendedAttributeRecordLength; // Bytes - this field refers to the
                                            // Extended Attribute Record, which provides
                                            // additional information about a file to
                                            // systems that know how to use it. Since
                                            // few systems use it, we will not discuss
                                            // it here. Refer to ISO 9660:1988 for
                                            // more information.
    unsigned long LocationOfExtentL;        // This is the Logical Block Number of the
                                            // first Logical Block allocated to the file
    unsigned long LocationOfExtentM;        // This is the Logical Block Number of the
                                            // first Logical Block allocated to the file
    unsigned long DataLengthL;              // Length of the file section in bytes
    unsigned long DataLengthM;              // Length of the file section in bytes
    volumedatetime_struct RecordingDateAndTime;    //
    unsigned char FileFlags;                // One Byte, each bit of which is a Flag:
                                            // bit
                                            // 0 File is Hidden if this bit is 1
                                            // 1 Entry is a Directory if this bit is 1
                                            // 2 Entry is an Associated file is this bit is 1
                                            // 3 Information is structured according to
                                            //   the extended attribute record if this
                                            //   bit is 1
                                            // 4 Owner, group and permissions are
                                            //   specified in the extended attribute
                                            //   record if this bit is 1
                                            // 5 Reserved (0)
                                            // 6 Reserved (0)
                                            // 7 File has more than one directory record
                                            //   if this bit is 1
    unsigned char FileUnitSize;             // This field is only valid if the file is
                                            // recorded in interleave mode
                                            // Otherwise this field is (00)
    unsigned char InterleaveGapSize;        // This field is only valid if the file is
                                            // recorded in interleave mode
                                            // Otherwise this field is (00)
    int VolumeSequenceNumber;               // The ordinal number of the volume in the Volume
                                            // Set on which the file described by the
                                            // directory record is recorded.
    unsigned char LengthOfFileIdentifier;   // Length of File Identifier (LEN_FI)
    unsigned char FileIdentifier[32]; // in theory this could be larger, but
                                      // since I'm dealing with saturn games
                                      // only, it'll be 8.3 format
    xadirrec_struct XAAttributes;
    unsigned long ParentRecord;  // If 0, we're root
} dirrec_struct;

typedef struct
{
    unsigned char LengthOfDirectoryIdentifier;  // Length of Directory Identifier (LEN_DI)
    unsigned char ExtendedAttributeRecordLength;// If an Extended Attribute Record is
                                                // recorded, this is the length in Bytes.
                                                // Otherwise, this is (00)
    unsigned long  LocationOfExtent;            // Logical Block Number of the first Logical
                                                // Block allocated to the Directory
    unsigned short ParentDirectoryNumber;       // The record number in the Path Table for
                                                // the parent directory of this directory
    unsigned char DirectoryIdentifier[32];      // This field is the same as in the Directory
                                                // Record
} ptr_struct;

typedef struct
{
    unsigned char VolumeDescriptorType;
    unsigned char StandardIdentifier[5];        // CD001
    unsigned char VolumeDescriptorVersion;
    unsigned char Ununsed;
    unsigned char SystemIdentifier[32];
    unsigned char VolumeIdentifier[32];
    unsigned char Unused2[8];
    unsigned long VolumeSpaceSizeL;             // Number of logical blocks in the Volume
    unsigned long VolumeSpaceSizeM;             // Number of logical blocks in the Volume
    unsigned char Unused3[32];
    unsigned short VolumeSetSizeL;              // The assigned Volume Set size of the Volume
    unsigned short VolumeSetSizeM;              // The assigned Volume Set size of the Volume
    unsigned short VolumeSequenceNumberL;       // The ordinal number of the volume in
                                                // the Volume Set
    unsigned short VolumeSequenceNumberM;       // The ordinal number of the volume in
                                                // the Volume Set
    unsigned short LogicalBlockSizeL;           // The size in bytes of a Logical Block
    unsigned short LogicalBlockSizeM;           // The size in bytes of a Logical Block
    unsigned long PathTableSizeL;               // Length in bytes of the path table
    unsigned long PathTableSizeM;               // Length in bytes of the path table
    unsigned long LocationOfTypeLPathTable;     // Logical Block Number of first Block allocated
                                                // to the Type L Path Table
    unsigned long LocationOfOptionalTypeLPathTable; // 0 if Optional Path Table was not recorded,
                                                // otherwise, Logical Block Number of first
                                                // Block allocated to the Optional Type L
                                                // Path Table
    unsigned long LocationOfTypeMPathTable;     // Logical Block Number of first Block
                                                // allocated to the Type M
    unsigned long LocationOfOptionalTypeMPathTable; // 0 if Optional Path Table was not
                                                // recorded, otherwise, Logical Path Table,
                                                // Block Number of first Block allocated to the
                                                // Type M Path Table.
    dirrec_struct DirectoryRecordForRootDirectory; // This is the actual directory record for
                                                // the top of the directory structure
    unsigned char VolumeSetIdentifier[128];     // Name of the multiple volume set of which
                                                // this volume is a member.
    unsigned char PublisherIdentifier[128];     // Identifies who provided the actual data
                                                // contained in the files. acharacters allowed.
    unsigned char DataPreparerIdentifier[128];  // Identifies who performed the actual
                                                // creation of the current volume.
    unsigned char ApplicationIdentifier[128];   // Identifies the specification of how the
                                                // data in the files are recorded.
    unsigned char CopyrightFileIdentifier[37];  // Identifies the file in the root directory
                                                // that contains the copyright notice for
                                                // this volume
    unsigned char AbstractFileIdentifier[37];   // Identifies the file in the root directory
                                                // that contains the abstract statement for
                                                // this volume
    unsigned char BibliographicFileIdentifier[37]; // Identifies the file in the root directory
                                                // that contains bibliographic records.
    unsigned char VolumeCreationDateAndTime[17];// Date and time at which the volume was created
    unsigned char VolumeModificationDateAndTime[17]; // Date and time at which the volume was
                                                // last modified
    unsigned char VolumeExpirationDateAndTime[17]; // Date and Time at which the information in
                                                // the volume may be considered obsolete.
    unsigned char VolumeEffectiveDateAndTime[17];// Date and Time at which the information
                                                // in the volume may be used
    unsigned char FileStructureVersion;         // 1
    unsigned char ReservedForFutureStandardization; // 0
    unsigned char ApplicationUse[512];          // This field is reserved for application use.
                                                // Its content is not specified by ISO-9660.
    unsigned char ReservedForFutureStandardization2[653]; // 0
} pvd_struct; // sizeof( PrimaryVolumeDescriptor ) must be 2048

typedef struct
{
	unsigned char fn; // file number
	unsigned char cn; // channel number
	unsigned char sm; // xa flag
	unsigned char ci; // coding information
} xa_subheader_struct;

#endif
