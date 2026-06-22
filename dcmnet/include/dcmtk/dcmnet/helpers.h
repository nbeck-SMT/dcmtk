/*
 *
 *  Copyright (C) 2021-2026, OFFIS e.V.
 *  All rights reserved.  See COPYRIGHT file for details.
 *
 *  This software and supporting documentation were partly developed by
 *
 *    OFFIS e.V.
 *    R&D Division Health
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *
 *  Module: dcmnet
 *
 *  Author: Michael Onken
 *
 *  Purpose: Collection of helper functions
 *
 */

#ifndef DCMNET_HELPERS_H
#define DCMNET_HELPERS_H

#include "dcmtk/ofstd/ofcond.h"
#include "dcmtk/dcmnet/dulstruc.h"

struct T_ASC_Parameters;
class LST_HEAD;

/** Deep-frees a single presentation context item, including its
 *  transferSyntaxList and all enqueued transfer syntax sub-items, and finally
 *  the context itself. Tolerates a NULL transferSyntaxList (e.g. when parsing
 *  failed before the list was created). Used to release a partially parsed
 *  context that has not (yet) been adopted by a presentation context list.
 *  @param ctx Address of the context pointer to free; set to NULL afterwards.
 *             A NULL *ctx is handled gracefully.
 */
void
destroyPresentationContext(PRV_PRESENTATIONCONTEXTITEM ** ctx);

/** Destroys presentationContextList as used in dul_associatepdu
 *  @param pcList The presentation context list to free (must not be NULL)
 */
void
destroyAssociatePDUPresentationContextList(LST_HEAD ** pcList);


/** Destroys userInfo as used in dul_associatepdu
 *  @param userInfo The user information lists to free (must not be NULL)
 */
void
destroyUserInformationLists(DUL_USERINFO * userInfo);

#endif
