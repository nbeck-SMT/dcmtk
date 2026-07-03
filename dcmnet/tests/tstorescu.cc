/*
 *
 *  Copyright (C) 2026, Open Connections GmbH
 *  All rights reserved.  See COPYRIGHT file for details.
 *
 *  This software and supporting documentation were developed by
 *
 *    OFFIS e.V.
 *    R&D Division Health
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *
 *  Module:  dcmnet
 *
 *  Author:  Michael Onken
 *
 *  Purpose: test program for DcmStorageSCU
 *
 */


#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#include "dcmtk/ofstd/oftest.h"
#include "dcmtk/dcmnet/dstorscu.h"


// Validation of Referenced File IDs read from a (possibly malicious) DICOMDIR
// by DcmStorageSCU::isReferencedFileIDSafe(). Only a conformant DICOM File ID
// (backslash-separated, non-empty components of uppercase letters, digits and
// underscore) may be accepted; everything that could escape the DICOMDIR
// directory (path traversal) must be rejected.
OFTEST(dcmnet_storageSCU_referencedFileIDSafety)
{
    // conformant, relative File IDs -- these must be accepted
    const char *validIDs[] =
    {
        "IMG00001",                 // single component
        "IMG001\\IMG00001",         // two components
        "DICOM\\IMAGES\\IM000001",  // three components
        "A\\B\\C\\D\\E\\F\\G\\H",    // eight single-character components
        "REPORT_1\\SCAN_02",        // underscores and digits
        "0",                        // a single digit
        "PATIENT_1"                 // underscore inside a component
    };
    // unsafe or malformed values -- these must be rejected
    const char *invalidIDs[] =
    {
        "",                         // empty value
        "..",                       // parent reference
        "..\\OUTDIR\\SECRET",       // leading parent reference (path traversal)
        "FOO\\..\\BAR",             // embedded parent reference
        "\\ABS\\SECRET",            // leading backslash -> absolute path
        "FOO\\BAR\\",               // trailing backslash -> empty last component
        "FOO\\\\BAR",               // double backslash -> empty component
        "/etc/passwd",              // POSIX absolute path
        "C:\\WINDOWS\\SYSTEM32",    // Windows drive-letter path
        "FILE.DCM",                 // '.' is not part of the File ID character set
        "FOO/BAR",                  // '/' is not a valid separator here
        "FOO BAR",                  // space is not allowed
        "img001",                   // lowercase letters are not allowed
        "FILE-01"                   // '-' is not part of the File ID character set
    };
    size_t i;
    for (i = 0; i < sizeof(validIDs) / sizeof(validIDs[0]); ++i)
    {
        if (!DcmStorageSCU::isReferencedFileIDSafe(validIDs[i]))
            OFCHECK_FAIL("valid Referenced File ID rejected: \"" << validIDs[i] << "\"");
    }
    for (i = 0; i < sizeof(invalidIDs) / sizeof(invalidIDs[0]); ++i)
    {
        if (DcmStorageSCU::isReferencedFileIDSafe(invalidIDs[i]))
            OFCHECK_FAIL("unsafe Referenced File ID accepted: \"" << invalidIDs[i] << "\"");
    }
}
