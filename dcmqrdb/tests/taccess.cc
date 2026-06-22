/*
 *
 *  Copyright (C) 2026, OFFIS e.V.
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
 *  Module:  dcmqrdb
 *
 *  Author:  Michael Onken
 *
 *  Purpose: test program for the per-AE "Access" mode predicates
 *           (writableStorageArea / readableStorageArea)
 *
 */

#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#include "dcmtk/ofstd/oftest.h"
#include "dcmtk/ofstd/oftempf.h"
#include "dcmtk/ofstd/offile.h"
#include "dcmtk/dcmqrdb/dcmqrcnf.h"

/* A minimal configuration file exercising every documented "Access" mode.
 * The storage-area paths need not exist: init() only parses the file, it
 * does not touch the filesystem entries.
 */
static const char *CONFIG_TEXT =
    "NetworkTCPPort  = 11112\n"
    "MaxPDUSize      = 16384\n"
    "MaxAssociations = 16\n"
    "HostTable BEGIN\n"
    "HostTable END\n"
    "VendorTable BEGIN\n"
    "VendorTable END\n"
    "AETable BEGIN\n"
    "AXS_R   /tmp/qr_axs_r    R   (10, 24mb) ANY\n"
    "AXS_RW  /tmp/qr_axs_rw   RW  (10, 24mb) ANY\n"
    "AXS_W   /tmp/qr_axs_w    W   (10, 24mb) ANY\n"
    "AXS_WR  /tmp/qr_axs_wr   WR  (10, 24mb) ANY\n"
    "AETable END\n";

/* Write CONFIG_TEXT to a temporary file and load it into "config".
 * Returns OFTrue on success.
 */
static OFBool loadTestConfig(OFTempFile &temp, DcmQueryRetrieveConfig &config)
{
    if (temp.getStatus().bad())
    {
        OFCHECK_FAIL("Could not create temporary file: " << temp.getStatus().text());
        return OFFalse;
    }

    OFFile f;
    if (!f.fopen(temp.getFilename(), "wb"))
    {
        OFCHECK_FAIL("Could not open temporary configuration file for writing");
        return OFFalse;
    }
    const size_t len = strlen(CONFIG_TEXT);
    const OFBool written = (f.fwrite(CONFIG_TEXT, 1, len) == len);
    f.fclose();
    if (!written)
    {
        OFCHECK_FAIL("Could not write temporary configuration file");
        return OFFalse;
    }

    /* init() returns 1 on success, 0 on error */
    if (config.init(temp.getFilename()) != 1)
    {
        OFCHECK_FAIL("Could not parse temporary configuration file");
        return OFFalse;
    }
    return OFTrue;
}

OFTEST(dcmqrdb_config_writableStorageArea)
{
    OFTempFile temp;
    DcmQueryRetrieveConfig config;
    if (!loadTestConfig(temp, config)) return;

    /* writable means C-STORE is permitted: "RW", "WR" and "W" */
    OFCHECK(!config.writableStorageArea("AXS_R"));
    OFCHECK( config.writableStorageArea("AXS_RW"));
    OFCHECK( config.writableStorageArea("AXS_W"));
    OFCHECK( config.writableStorageArea("AXS_WR"));
}

OFTEST(dcmqrdb_config_readableStorageArea)
{
    OFTempFile temp;
    DcmQueryRetrieveConfig config;
    if (!loadTestConfig(temp, config)) return;

    /* readable means query/retrieve is permitted: "R", "RW" and "WR".
     * The "W" (write-only) area must NOT be readable -- this is the access
     * control gap that the readableStorageArea() predicate closes.
     */
    OFCHECK( config.readableStorageArea("AXS_R"));
    OFCHECK( config.readableStorageArea("AXS_RW"));
    OFCHECK(!config.readableStorageArea("AXS_W"));
    OFCHECK( config.readableStorageArea("AXS_WR"));
}
