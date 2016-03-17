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

// Must be C callable
CPL_C_START
    void CPL_DLL RegisterOGRAOI();
CPL_C_END

static int OGRAOIDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    // first, is it .aoi?
    if( !EQUAL( CPLGetExtension(poOpenInfo->pszFilename), "aoi" ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Verify that this is a HFA file.                                 */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 15
        || !EQUALN((char *) poOpenInfo->pabyHeader,"EHFA_HEADER_TAG",15) )
        return FALSE;
    else
        return TRUE;
}

/* Try opening the datasource as an AOI */
/* If successful, return a pointer to a new instance of OGRAOIDataSource */
/* otherwise return NULL */
static GDALDataset* OGRAOIDriverOpen(GDALOpenInfo* poOpenInfo)
{
    if( !OGRAOIDriverIdentify(poOpenInfo) )
        return NULL;

    OGRAOIDataSource *poDS = new OGRAOIDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, 
                poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        return NULL;
    }
    else
    {
        return poDS;
    }
}

void RegisterOGRAOI()
{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "AOI" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "AOI" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "ERDAS Imagine AOI" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "aoi" );

        poDriver->pfnIdentify = OGRAOIDriverIdentify;
        poDriver->pfnOpen = OGRAOIDriverOpen;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
