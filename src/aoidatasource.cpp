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
#include "aoidatasource.h"
#include "aoilayer.h"


/* from hfaopen.cpp - unfortunately declared static so we can't get access*/
/* had to copy and paste into here */
/* I found if this isn't called we get a crash reading fields */
/************************************************************************/
/*                          HFAGetDictionary()                          */
/************************************************************************/

static char * HFAGetDictionary( HFAHandle hHFA )

{
    int		nDictMax = 100;
    char	*pszDictionary = (char *) CPLMalloc(nDictMax);
    int		nDictSize = 0;

    VSIFSeekL( hHFA->fp, hHFA->nDictionaryPos, SEEK_SET );

    while( TRUE )
    {
        if( nDictSize >= nDictMax-1 )
        {
            nDictMax = nDictSize * 2 + 100;
            pszDictionary = (char *) CPLRealloc(pszDictionary, nDictMax );
        }

        if( VSIFReadL( pszDictionary + nDictSize, 1, 1, hHFA->fp ) < 1
            || pszDictionary[nDictSize] == '\0'
            || (nDictSize > 2 && pszDictionary[nDictSize-2] == ','
                && pszDictionary[nDictSize-1] == '.') )
            break;

        nDictSize++;
    }

    pszDictionary[nDictSize] = '\0';


    return( pszDictionary );
}

// Constructor - no layers etc until Open() called
OGRAOIDataSource::OGRAOIDataSource()
{
    m_poLayer = NULL;
    m_nLayers = 0;

    m_pszName = NULL;
    m_psInfo = NULL;
}

// Destructor - free memory
OGRAOIDataSource::~OGRAOIDataSource()
{
    if( m_poLayer != NULL )
        delete m_poLayer;

    CPLFree( m_pszName );

    if( m_psInfo != NULL )
    {
        VSIFCloseL( m_psInfo->fp );
        CPLFree( m_psInfo->pszFilename );
        CPLFree( m_psInfo->pszPath );
        delete m_psInfo->poRoot;
        CPLFree( m_psInfo->pszDictionary );
        delete m_psInfo->poDictionary;
        CPLFree( m_psInfo );
    }
}

// Return TRUE if it is an aoi file and we will be able to open it
// Adapted from HFAOpen()
int OGRAOIDataSource::Open( const char *pszFilename, int bUpdate )
{
    VSILFILE *fp;
    char	szHeader[16];
    GUInt32	nHeaderPos;
// -------------------------------------------------------------------- 
//      Does this appear to be an .aoi file?                           
// --------------------------------------------------------------------
    if( !EQUAL( CPLGetExtension(pszFilename), "aoi" ) )
        return FALSE;

    if( bUpdate )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Update access not supported by the AOI driver." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "rb" );

    /* should this be changed to use some sort of CPLFOpen() which will
       set the error? */
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "File open of %s failed.",
                  pszFilename );

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Read and verify the header.                                     */
/* -------------------------------------------------------------------- */
    if( VSIFReadL( szHeader, 16, 1, fp ) < 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to read 16 byte header failed for\n%s.",
                  pszFilename );

        return FALSE;
    }

    if( !EQUALN(szHeader,"EHFA_HEADER_TAG",15) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File %s is not an Imagine HFA file ... header wrong.",
                  pszFilename );

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create the HFAInfo_t                                            */
/* -------------------------------------------------------------------- */
    m_psInfo = (HFAInfo_t *) CPLCalloc(sizeof(HFAInfo_t),1);

    m_psInfo->pszFilename = CPLStrdup(CPLGetFilename(pszFilename));
    m_psInfo->pszPath = CPLStrdup(CPLGetPath(pszFilename));
    m_psInfo->fp = fp;
	m_psInfo->eAccess = HFA_ReadOnly;
    m_psInfo->bTreeDirty = FALSE;

/* -------------------------------------------------------------------- */
/*	Where is the header?						*/
/* -------------------------------------------------------------------- */
    VSIFReadL( &nHeaderPos, sizeof(GInt32), 1, fp );
    HFAStandard( 4, &nHeaderPos );

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fp, nHeaderPos, SEEK_SET );

    VSIFReadL( &(m_psInfo->nVersion), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(m_psInfo->nVersion) );

    VSIFReadL( szHeader, 4, 1, fp ); /* skip freeList */

    VSIFReadL( &(m_psInfo->nRootPos), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(m_psInfo->nRootPos) );

    VSIFReadL( &(m_psInfo->nEntryHeaderLength), sizeof(GInt16), 1, fp );
    HFAStandard( 2, &(m_psInfo->nEntryHeaderLength) );

    VSIFReadL( &(m_psInfo->nDictionaryPos), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(m_psInfo->nDictionaryPos) );

/* -------------------------------------------------------------------- */
/*      Collect file size.                                              */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fp, 0, SEEK_END );
    m_psInfo->nEndOfFile = (GUInt32) VSIFTellL( fp );

/* -------------------------------------------------------------------- */
/*      Instantiate the root entry.                                     */
/* -------------------------------------------------------------------- */
    m_psInfo->poRoot = new HFAEntry( m_psInfo, m_psInfo->nRootPos, NULL, NULL );

/* -------------------------------------------------------------------- */
/*      Read the dictionary                                             */
/* -------------------------------------------------------------------- */
    m_psInfo->pszDictionary = HFAGetDictionary( m_psInfo );
    m_psInfo->poDictionary = new HFADictionary( m_psInfo->pszDictionary );


/* -------------------------------------------------------------------- */
/*      See if this file has an AOInode                                 */
/* -------------------------------------------------------------------- */
    HFAEntry *pAOInode = m_psInfo->poRoot->GetNamedChild("AOInode");
    if( pAOInode == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to find AOInode in %s.",
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create layers for each geometry type                            */
/*  AOI Files contain multiple types, so we put them in seperate layers */
/*  AFAIK OGR only supports one geometry type per layer                 */
/* -------------------------------------------------------------------- */
    m_nLayers = 1;
    m_poLayer = new OGRAOILayer( pAOInode, CPLGetBasename( pszFilename ) );

    m_pszName = CPLStrdup( pszFilename );

    return TRUE;
}

// Get a specified layer
OGRLayer *OGRAOIDataSource::GetLayer( int iLayer )
{
    if( iLayer < 0 || iLayer >= m_nLayers )
        return NULL;
    else
        return m_poLayer;
}
