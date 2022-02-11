/* ******************************************************************************
 * Copyright (c) 2015, Sam Gillingham <gillingham.sam@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "hfa_p.h"
#include "hfa.h"
#include <ogr_spatialref.h>
#include <vector>
#include "aoiproj.h"

/************************************************************************/
/*                  CreateSpatialReference                              */
/* Adapted from HFADataset::ReadProjection                              */
/************************************************************************/

std::unique_ptr<OGRSpatialReference> CreateSpatialReference( HFAEntry* pAOInode )
{
    const Eprj_Datum	      *psDatum;
    const Eprj_ProParameters  *psPro;
    const Eprj_MapInfo        *psMapInfo;
    std::unique_ptr<OGRSpatialReference> SRS;

// Each AOI has a projection (which are all the same). Just get the first 
// one and use that
// Possibly should use a search that just stops at the first rather
// than finding all the matching entries.
    std::vector<HFAEntry*> antInfoVector = 
        pAOInode->FindChildren("antInfo", "AntHeader_Eant");

    if( antInfoVector.empty() )
        return NULL;

    HFAEntry *poAntNode = antInfoVector.front();

/* -------------------------------------------------------------------- */
/*      General case for Erdas style projections.                       */
/*                                                                      */
/*      We make a particular effort to adapt the mapinfo->proname as    */
/*      the PROJCS[] name per #2422.                                    */
/* -------------------------------------------------------------------- */
    psDatum = AOIGetDatum( poAntNode );
    psPro = AOIGetProParameters( poAntNode );
    psMapInfo = AOIGetMapInfo( poAntNode );

    HFAEntry *poMapInformation = NULL;
    if( psMapInfo == NULL ) 
    {
        poMapInformation = poAntNode->GetNamedChild("MapInformation");
    }

    if( !psDatum || !psPro ||
        (psMapInfo == NULL && poMapInformation == NULL) ||
        ((strlen(psDatum->datumname) == 0 || EQUAL(psDatum->datumname, "Unknown")) && 
        (strlen(psPro->proName) == 0 || EQUAL(psPro->proName, "Unknown")) &&
        (psMapInfo && (strlen(psMapInfo->proName) == 0 || EQUAL(psMapInfo->proName, "Unknown"))) && 
        psPro->proZone == 0) )
    {
        /* do nothing */
    }
    else
    {
        SRS = HFAPCSStructToOSR( psDatum, psPro, psMapInfo, 
                                       poMapInformation );
    }

    if( psPro != NULL )
    {
        CPLFree( psPro->proExeName );
        CPLFree( psPro->proName );
        CPLFree( psPro->proSpheroid.sphereName );
        CPLFree( (void*)psPro );
    }

    if( psDatum != NULL )
    {
        CPLFree( psDatum->datumname );
        CPLFree( psDatum->gridname );
        CPLFree( (void*)psDatum );
    }

    if( psMapInfo != NULL )
    {
        CPLFree( psMapInfo->proName );
        CPLFree( psMapInfo->units );
        CPLFree( (void*)psMapInfo );
    }
    return SRS;
}

/************************************************************************/
/*                        AOIGetProParameters()                         */
/*  Adapted from hfaopen.cpp HFAGetProParameters().                     */
/************************************************************************/

const Eprj_ProParameters *AOIGetProParameters( HFAEntry *poAntNode )
{
    HFAEntry	*poMIEntry;
    Eprj_ProParameters *psProParms;
    int		i;

/* -------------------------------------------------------------------- */
/*      Get the HFA node.                                               */
/* -------------------------------------------------------------------- */
    poMIEntry = poAntNode->GetNamedChild( "Projection" );
    if( poMIEntry == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Allocate the structure.                                         */
/* -------------------------------------------------------------------- */
    psProParms = (Eprj_ProParameters *)CPLCalloc(sizeof(Eprj_ProParameters),1);

/* -------------------------------------------------------------------- */
/*      Fetch the fields.                                               */
/* -------------------------------------------------------------------- */
    psProParms->proType = (Eprj_ProType) poMIEntry->GetIntField("proType");
    psProParms->proNumber = poMIEntry->GetIntField("proNumber");
    psProParms->proExeName =CPLStrdup(poMIEntry->GetStringField("proExeName"));
    psProParms->proName = CPLStrdup(poMIEntry->GetStringField("proName"));
    psProParms->proZone = poMIEntry->GetIntField("proZone");

    for( i = 0; i < 15; i++ )
    {
        char	szFieldName[40];

        sprintf( szFieldName, "proParams[%d]", i );
        psProParms->proParams[i] = poMIEntry->GetDoubleField(szFieldName);
    }

    psProParms->proSpheroid.sphereName =
        CPLStrdup(poMIEntry->GetStringField("proSpheroid.sphereName"));
    psProParms->proSpheroid.a = poMIEntry->GetDoubleField("proSpheroid.a");
    psProParms->proSpheroid.b = poMIEntry->GetDoubleField("proSpheroid.b");
    psProParms->proSpheroid.eSquared =
        poMIEntry->GetDoubleField("proSpheroid.eSquared");
    psProParms->proSpheroid.radius =
        poMIEntry->GetDoubleField("proSpheroid.radius");

    return psProParms;
}

/************************************************************************/
/*                            AOIGetDatum()                             */
/*                       Adapted from HFAGetDatum()                     */
/************************************************************************/

const Eprj_Datum *AOIGetDatum( HFAEntry *poAntNode )
{
    HFAEntry	*poMIEntry;
    Eprj_Datum	*psDatum;
    int		i;

/* -------------------------------------------------------------------- */
/*      Get the HFA node.                                               */
/* -------------------------------------------------------------------- */
    poMIEntry = poAntNode->GetNamedChild( "Projection.Datum" );
    if( poMIEntry == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Allocate the structure.                                         */
/* -------------------------------------------------------------------- */
    psDatum = (Eprj_Datum *) CPLCalloc(sizeof(Eprj_Datum),1);

/* -------------------------------------------------------------------- */
/*      Fetch the fields.                                               */
/* -------------------------------------------------------------------- */
    psDatum->datumname = CPLStrdup(poMIEntry->GetStringField("datumname"));
    psDatum->type = (Eprj_DatumType) poMIEntry->GetIntField("type");

    for( i = 0; i < 7; i++ )
    {
        char	szFieldName[30];

        sprintf( szFieldName, "params[%d]", i );
        psDatum->params[i] = poMIEntry->GetDoubleField(szFieldName);
    }

    psDatum->gridname = CPLStrdup(poMIEntry->GetStringField("gridname"));

    return psDatum;
}


/************************************************************************/
/*                           AOIGetMapInfo()                            */
/*                        Adapted from HFAGetMapInfo                    */
/************************************************************************/

const Eprj_MapInfo *AOIGetMapInfo( HFAEntry *poAntNode )
{
    HFAEntry	*poMIEntry;
    Eprj_MapInfo *psMapInfo;

/* -------------------------------------------------------------------- */
/*      Get the HFA node.  If we don't find it under the usual name     */
/*      we search for any node of the right type (#3338).               */
/* -------------------------------------------------------------------- */
    poMIEntry = poAntNode->GetNamedChild( "Map_Info" );
    if( poMIEntry == NULL )
    {
        HFAEntry *poChild;
        for( poChild = poAntNode->GetChild();
             poChild != NULL && poMIEntry == NULL;
             poChild = poChild->GetNext() )
        {
            if( EQUAL(poChild->GetType(),"Eprj_MapInfo") )
                poMIEntry = poChild;
        }
    }

    if( poMIEntry == NULL )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Allocate the structure.                                         */
/* -------------------------------------------------------------------- */
    psMapInfo = (Eprj_MapInfo *) CPLCalloc(sizeof(Eprj_MapInfo),1);

/* -------------------------------------------------------------------- */
/*      Fetch the fields.                                               */
/* -------------------------------------------------------------------- */
    CPLErr eErr;

    psMapInfo->proName = CPLStrdup(poMIEntry->GetStringField("proName"));

    psMapInfo->upperLeftCenter.x =
        poMIEntry->GetDoubleField("upperLeftCenter.x");
    psMapInfo->upperLeftCenter.y =
        poMIEntry->GetDoubleField("upperLeftCenter.y");

    psMapInfo->lowerRightCenter.x =
        poMIEntry->GetDoubleField("lowerRightCenter.x");
    psMapInfo->lowerRightCenter.y =
        poMIEntry->GetDoubleField("lowerRightCenter.y");

   psMapInfo->pixelSize.width =
       poMIEntry->GetDoubleField("pixelSize.width",&eErr);
   psMapInfo->pixelSize.height =
       poMIEntry->GetDoubleField("pixelSize.height",&eErr);

   // The following is basically a hack to get files with 
   // non-standard MapInfo's that misname the pixelSize fields. (#3338)
   if( eErr != CE_None )
   {
       psMapInfo->pixelSize.width =
           poMIEntry->GetDoubleField("pixelSize.x");
       psMapInfo->pixelSize.height =
           poMIEntry->GetDoubleField("pixelSize.y");
   }

   psMapInfo->units = CPLStrdup(poMIEntry->GetStringField("units"));

   return psMapInfo;
}

// Adapted from HFAEvaluateXFormStack()
// I've only ever seen order == 1, but handle 2 and 3 just in case
// order = 0 is an empty polynomial (ie an error when readig etc)
void ApplyXformPolynomial( Efga_Polynomial *pPoly, double *pdfX, double *pdfY )
{
    double dfXOut, dfYOut;

    if( pPoly->order == 0 )
        return;

    else if( pPoly->order == 1 )
    {
        dfXOut = pPoly->polycoefvector[0] 
            + pPoly->polycoefmtx[0] * *pdfX
            + pPoly->polycoefmtx[2] * *pdfY;

        dfYOut = pPoly->polycoefvector[1] 
            + pPoly->polycoefmtx[1] * *pdfX
            + pPoly->polycoefmtx[3] * *pdfY;

        *pdfX = dfXOut;
        *pdfY = dfYOut;
    }

    else if( pPoly->order == 2 )
    {
        dfXOut = pPoly->polycoefvector[0] 
            + pPoly->polycoefmtx[0] * *pdfX
            + pPoly->polycoefmtx[2] * *pdfY
            + pPoly->polycoefmtx[4] * *pdfX * *pdfX
            + pPoly->polycoefmtx[6] * *pdfX * *pdfY
            + pPoly->polycoefmtx[8] * *pdfY * *pdfY;
        dfYOut = pPoly->polycoefvector[1] 
            + pPoly->polycoefmtx[1] * *pdfX
            + pPoly->polycoefmtx[3] * *pdfY
            + pPoly->polycoefmtx[5] * *pdfX * *pdfX
            + pPoly->polycoefmtx[7] * *pdfX * *pdfY
            + pPoly->polycoefmtx[9] * *pdfY * *pdfY;

        *pdfX = dfXOut;
        *pdfY = dfYOut;
    }

    else if( pPoly->order == 3 )
    {
       dfXOut = pPoly->polycoefvector[0] 
            + pPoly->polycoefmtx[ 0] * *pdfX
            + pPoly->polycoefmtx[ 2] * *pdfY
            + pPoly->polycoefmtx[ 4] * *pdfX * *pdfX
            + pPoly->polycoefmtx[ 6] * *pdfX * *pdfY
            + pPoly->polycoefmtx[ 8] * *pdfY * *pdfY
            + pPoly->polycoefmtx[10] * *pdfX * *pdfX * *pdfX
            + pPoly->polycoefmtx[12] * *pdfX * *pdfX * *pdfY
            + pPoly->polycoefmtx[14] * *pdfX * *pdfY * *pdfY
            + pPoly->polycoefmtx[16] * *pdfY * *pdfY * *pdfY;
        dfYOut = pPoly->polycoefvector[1] 
            + pPoly->polycoefmtx[ 1] * *pdfX
            + pPoly->polycoefmtx[ 3] * *pdfY
            + pPoly->polycoefmtx[ 5] * *pdfX * *pdfX
            + pPoly->polycoefmtx[ 7] * *pdfX * *pdfY
            + pPoly->polycoefmtx[ 9] * *pdfY * *pdfY
            + pPoly->polycoefmtx[11] * *pdfX * *pdfX * *pdfX
            + pPoly->polycoefmtx[13] * *pdfX * *pdfX * *pdfY
            + pPoly->polycoefmtx[15] * *pdfX * *pdfY * *pdfY
            + pPoly->polycoefmtx[17] * *pdfY * *pdfY * *pdfY;

        *pdfX = dfXOut;
        *pdfY = dfYOut;
    }
}

// adapted from HFAReadAndValidatePoly()
// Given the Element_2_Eant structure, read out the transform polynomial
void ReadXformPolynomial( HFAEntry *pElement, Efga_Polynomial *pPoly )
{
    memset( pPoly, 0, sizeof(Efga_Polynomial) );

    // note: GetIntField returns 0 on failure
    // we treat pPoly->order == 0 as invalid polynomial and ignore it
    int termcount;
    pPoly->order = pElement->GetIntField("xformMatrix.order");
    //numdimtransform = pElement->GetIntField("xformMatrix.numdimtransform");
    //numdimpolynomial = pElement->GetIntField("xformMatrix.numdimpolynomial");
    termcount = pElement->GetIntField("xformMatrix.termcount");
    if( ( pPoly->order == 1 && termcount != 3) 
        || (pPoly->order == 2 && termcount != 6) 
        || (pPoly->order == 3 && termcount != 10) )
    {
        // something not right - ignore
        pPoly->order = 0;
    }
    else
    {
        // Get coefficients
        int i;
        CPLString osFldName;
        for( i = 0; i < termcount*2 - 2; i++ )
        {
            osFldName.Printf( "xformMatrix.polycoefmtx[%d]", i );
            pPoly->polycoefmtx[i] = pElement->GetDoubleField(osFldName);
        }

        for( i = 0; i < 2; i++ )
        {
            osFldName.Printf( "xformMatrix.polycoefvector[%d]", i );
            pPoly->polycoefvector[i] = pElement->GetDoubleField(osFldName);
        }
    }
}
