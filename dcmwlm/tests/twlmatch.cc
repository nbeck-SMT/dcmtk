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
 *  Module:  dcmwlm
 *
 *  Author:  Michael Onken
 *
 *  Purpose: Tests for the matching and return key handling of the
 *           wlmscpfs filesystem worklist data source, with focus on the
 *           veterinary patient attributes (species/breed code sequences,
 *           breed registration sequence and responsible organization,
 *           for now).
 *
 */

#include "dcmtk/config/osconfig.h"

#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcitem.h"
#include "dcmtk/dcmwlm/wldsfs.h"
#include "dcmtk/dcmwlm/wltypdef.h"
#include "dcmtk/ofstd/offile.h"
#include "dcmtk/ofstd/offilsys.h"
#include "dcmtk/ofstd/oflist.h"
#include "dcmtk/ofstd/ofstd.h"
#include "dcmtk/ofstd/ofstring.h"
#include "dcmtk/ofstd/oftempf.h"
#include "dcmtk/ofstd/oftest.h"

#ifdef _WIN32
#include <direct.h>
#endif
#ifdef HAVE_UNISTD_H
BEGIN_EXTERN_C
#include <unistd.h>
END_EXTERN_C
#endif

// AE title used as the worklist sub-directory; must be a valid filesystem AE title.
#define VET_AETITLE "VETWL"

// ----- Small filesystem helpers -------------------------------------------

static void removeDir(const OFString& path)
{
#ifdef _WIN32
    _rmdir(path.c_str());
#else
    rmdir(path.c_str());
#endif
}

// Create an empty file (used for the worklist lock file).
static OFBool touchFile(const OFString& path)
{
    OFFile f;
    if (!f.fopen(path.c_str(), "wb"))
        return OFFalse;
    f.fclose();
    return OFTrue;
}

// ----- Worklist record (.wl file) construction ----------------------------

// Build a single worklist file holding a veterinary patient with all
// attributes that this test exercises and write it to 'fileName' as a bare
// data set (no file meta information, like a real wlmscpfs .wl file).
static OFBool writeVetWorklistFile(const OFString& fileName)
{
    DcmFileFormat ff;
    DcmDataset* ds = ff.getDataset();

    // plain (non-sequence) patient attributes
    ds->putAndInsertString(DCM_PatientName, "Bello^Dog");
    ds->putAndInsertString(DCM_PatientID, "VET001");
    ds->putAndInsertString(DCM_PatientSpeciesDescription, "Canine");
    ds->putAndInsertString(DCM_PatientSexNeutered, "ALTERED");
    ds->putAndInsertString(DCM_PatientBreedDescription, "Beagle");
    ds->putAndInsertString(DCM_ResponsiblePerson, "Smith^John");
    ds->putAndInsertString(DCM_ResponsibleOrganization, "Happy Paws Clinic");

    // Patient Species Code Sequence (0010,2202): (448771007, SCT, "Canis lupus
    // familiaris"), a valid code from PS3.16 CID 7454 "Animal Taxonomic Rank Value".
    DcmItem* item = NULL;
    ds->findOrCreateSequenceItem(DCM_PatientSpeciesCodeSequence, item, 0);
    item->putAndInsertString(DCM_CodeValue, "448771007");
    item->putAndInsertString(DCM_CodingSchemeDesignator, "SCT");
    item->putAndInsertString(DCM_CodeMeaning, "Canis lupus familiaris");

    // Patient Breed Code Sequence (0010,2293): (132380002, SCT, "Beagle, Smooth
    // dog breed"), a valid code from PS3.16 CID 7480 "Breed".
    item = NULL;
    ds->findOrCreateSequenceItem(DCM_PatientBreedCodeSequence, item, 0);
    item->putAndInsertString(DCM_CodeValue, "132380002");
    item->putAndInsertString(DCM_CodingSchemeDesignator, "SCT");
    item->putAndInsertString(DCM_CodeMeaning, "Beagle, Smooth dog breed");

    // Breed Registration Sequence (0010,2294) with nested Breed Registry Code
    // Sequence (0010,2296): (109200, DCM, "America Kennel Club"), a valid code
    // from PS3.16 CID 7481 "Breed Registry".
    DcmItem* regItem = NULL;
    ds->findOrCreateSequenceItem(DCM_BreedRegistrationSequence, regItem, 0);
    regItem->putAndInsertString(DCM_BreedRegistrationNumber, "12345");
    DcmItem* regCodeItem = NULL;
    regItem->findOrCreateSequenceItem(DCM_BreedRegistryCodeSequence, regCodeItem, 0);
    regCodeItem->putAndInsertString(DCM_CodeValue, "109200");
    regCodeItem->putAndInsertString(DCM_CodingSchemeDesignator, "DCM");
    regCodeItem->putAndInsertString(DCM_CodeMeaning, "America Kennel Club");

    return ff
        .saveFile(fileName.c_str(),
                  EXS_LittleEndianExplicit,
                  EET_ExplicitLength,
                  EGL_recalcGL,
                  EPD_withoutPadding,
                  0,
                  0,
                  EWM_dataset)
        .good();
}

// ----- Query execution helpers --------------------------------------------

// Run a single C-FIND against the data source and collect all responses.
// The caller owns the returned data sets (see freeResults()).
static size_t runQuery(WlmDataSourceFileSystem& wdb, DcmDataset& mask, OFList<DcmDataset*>& results)
{
    WlmDataSourceStatusType status = wdb.StartFindRequest(mask);
    while (status == WLM_PENDING || status == WLM_PENDING_WARNING)
    {
        DcmDataset* rsp = wdb.NextFindResponse(status);
        if (rsp == NULL)
            break;
        results.push_back(rsp);
    }
    return results.size();
}

static void freeResults(OFList<DcmDataset*>& results)
{
    for (OFListIterator(DcmDataset*) it = results.begin(); it != results.end(); ++it)
        delete *it;
    results.clear();
}

// Convenience: get the string value of a (possibly nested) attribute.
static OFString seqItemString(DcmItem* item, const DcmTagKey& tag)
{
    OFString value;
    if (item != NULL)
        item->findAndGetOFString(tag, value);
    return value;
}

// Verify that a returned code item carries all the code fields that were
// stored in the worklist file (Code Value, Coding Scheme Designator and Code
// Meaning); the Coding Scheme Version is not stored and must come back empty.
static void checkCodeItem(DcmItem* item, const OFString& codeValue,
                          const OFString& designator, const OFString& meaning)
{
    OFCHECK(item != NULL);
    OFCHECK_EQUAL(seqItemString(item, DCM_CodeValue), codeValue);
    OFCHECK_EQUAL(seqItemString(item, DCM_CodingSchemeDesignator), designator);
    OFCHECK_EQUAL(seqItemString(item, DCM_CodeMeaning), meaning);
    OFCHECK_EQUAL(seqItemString(item, DCM_CodingSchemeVersion), OFString(""));
}

OFTEST(dcmwlm_vetmed_matching)
{
    // ----- Set up a temporary worklist database on disk. -----
    OFString tempBase;
    OFTempFile::getTempPath(tempBase);

    char suffix[64];
    OFStandard::snprintf(suffix, sizeof(suffix), "dcmwlm_vetmed_%ld", OFStandard::getProcessID());

    const OFString rootDir  = (OFpath(tempBase) / suffix).native();
    const OFString aeDir    = (OFpath(rootDir) / VET_AETITLE).native();
    const OFString lockFile = (OFpath(aeDir) / "lockfile").native();
    const OFString wlFile   = (OFpath(aeDir) / "vet001.wl").native();

    OFCHECK(OFStandard::createDirectory(rootDir, tempBase).good());
    OFCHECK(OFStandard::createDirectory(aeDir, rootDir).good());
    OFCHECK(touchFile(lockFile));
    OFCHECK(writeVetWorklistFile(wlFile));

    // ----- Connect the filesystem data source to that database. -----
    WlmDataSourceFileSystem wdb;
    wdb.SetDfPath(rootDir);
    wdb.SetCalledApplicationEntityTitle(VET_AETITLE);
    // keep the worklist file minimal: do not require all type 1 return keys
    wdb.SetEnableRejectionOfIncompleteWlFiles(OFFalse);
    OFCHECK(wdb.ConnectToDataSource().good());
    // This call propagates the called AE title to the filesystem interaction
    // manager (which selects the worklist sub-directory) and validates it; the
    // wlmscpfs activity manager performs the same step before each query.
    OFCHECK(wdb.IsCalledApplicationEntityTitleSupported());

    // ----- Test 1: empty sequences are expanded and returned. -----
    // A query carrying only empty (universal) return keys must match the single
    // record and the response must carry the expanded veterinary sequences,
    // including all code attributes of the species/breed code sequences, the
    // nested Breed Registry Code Sequence and the new Responsible Organization.
    {
        DcmDataset mask;
        mask.insertEmptyElement(DCM_PatientName);
        mask.insertEmptyElement(DCM_ResponsibleOrganization);
        mask.insertEmptyElement(DCM_PatientSpeciesCodeSequence);
        mask.insertEmptyElement(DCM_PatientBreedCodeSequence);
        mask.insertEmptyElement(DCM_BreedRegistrationSequence);

        OFList<DcmDataset*> results;
        OFCHECK(runQuery(wdb, mask, results) == 1);
        if (!results.empty())
        {
            DcmDataset* r = results.front();
            OFString value;

            OFCHECK(r->findAndGetOFString(DCM_ResponsibleOrganization, value).good());
            OFCHECK_EQUAL(value, OFString("Happy Paws Clinic"));

            // Patient Species Code Sequence: all stored code fields.
            DcmItem* speciesItem = NULL;
            OFCHECK(r->findAndGetSequenceItem(DCM_PatientSpeciesCodeSequence, speciesItem, 0).good());
            checkCodeItem(speciesItem, "448771007", "SCT", "Canis lupus familiaris");

            // Patient Breed Code Sequence: all stored code fields.
            DcmItem* breedItem = NULL;
            OFCHECK(r->findAndGetSequenceItem(DCM_PatientBreedCodeSequence, breedItem, 0).good());
            checkCodeItem(breedItem, "132380002", "SCT", "Beagle, Smooth dog breed");

            // Breed Registration Sequence with nested Breed Registry Code Sequence.
            DcmItem* regItem = NULL;
            OFCHECK(r->findAndGetSequenceItem(DCM_BreedRegistrationSequence, regItem, 0).good());
            OFCHECK_EQUAL(seqItemString(regItem, DCM_BreedRegistrationNumber), OFString("12345"));

            DcmItem* regCodeItem = NULL;
            if (regItem != NULL)
                OFCHECK(regItem->findAndGetSequenceItem(DCM_BreedRegistryCodeSequence, regCodeItem, 0).good());
            checkCodeItem(regCodeItem, "109200", "DCM", "America Kennel Club");
        }
        freeResults(results);
    }

    // ----- Test 2: plain veterinary attribute as matching key. -----
    {
        DcmDataset mask;
        mask.putAndInsertString(DCM_PatientSpeciesDescription, "Canine");
        OFList<DcmDataset*> results;
        OFCHECK(runQuery(wdb, mask, results) == 1);
        freeResults(results);
    }
    {
        DcmDataset mask;
        mask.putAndInsertString(DCM_PatientSpeciesDescription, "Feline");
        OFList<DcmDataset*> results;
        OFCHECK(runQuery(wdb, mask, results) == 0);
        freeResults(results);
    }

    // ----- Test 3: code sequence as matching key (Patient Breed Code Seq). -----
    {
        DcmDataset mask;
        DcmItem* item = NULL;
        mask.findOrCreateSequenceItem(DCM_PatientBreedCodeSequence, item, 0);
        item->putAndInsertString(DCM_CodeValue, "132380002");
        OFList<DcmDataset*> results;
        OFCHECK(runQuery(wdb, mask, results) == 1);
        freeResults(results);
    }
    {
        DcmDataset mask;
        DcmItem* item = NULL;
        mask.findOrCreateSequenceItem(DCM_PatientBreedCodeSequence, item, 0);
        item->putAndInsertString(DCM_CodeValue, "NOMATCH");
        OFList<DcmDataset*> results;
        OFCHECK(runQuery(wdb, mask, results) == 0);
        freeResults(results);
    }

    // ----- Test 4: nested code sequence as matching key. -----
    // Breed Registration Sequence > Breed Registry Code Sequence > Code Value.
    {
        DcmDataset mask;
        DcmItem* regItem = NULL;
        mask.findOrCreateSequenceItem(DCM_BreedRegistrationSequence, regItem, 0);
        DcmItem* regCodeItem = NULL;
        regItem->findOrCreateSequenceItem(DCM_BreedRegistryCodeSequence, regCodeItem, 0);
        regCodeItem->putAndInsertString(DCM_CodeValue, "109200");
        OFList<DcmDataset*> results;
        OFCHECK(runQuery(wdb, mask, results) == 1);
        freeResults(results);
    }
    {
        DcmDataset mask;
        DcmItem* regItem = NULL;
        mask.findOrCreateSequenceItem(DCM_BreedRegistrationSequence, regItem, 0);
        DcmItem* regCodeItem = NULL;
        regItem->findOrCreateSequenceItem(DCM_BreedRegistryCodeSequence, regCodeItem, 0);
        regCodeItem->putAndInsertString(DCM_CodeValue, "WRONG");
        OFList<DcmDataset*> results;
        OFCHECK(runQuery(wdb, mask, results) == 0);
        freeResults(results);
    }

    wdb.DisconnectFromDataSource();

    // ----- Clean up the temporary worklist database. -----
    OFStandard::deleteFile(wlFile);
    OFStandard::deleteFile(lockFile);
    removeDir(aeDir);
    removeDir(rootDir);
}
