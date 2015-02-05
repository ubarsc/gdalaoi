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

#include "aoidriver.h"
#include "aoidatasource.h"

// see http://www.gdal.org/ogr/ogr_drivertut.html

// Must be C callable
CPL_C_START
    void CPL_DLL RegisterOGRAOI();
CPL_C_END

void RegisterOGRAOI()
{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRAOIDriver() );
}

OGRAOIDriver::~OGRAOIDriver()
{
}

// Returns the name I have given this driver
const char *OGRAOIDriver::GetName()
{
    return "ERDAS Imagine AOI";
}

/* Try opening the datasource as an AOI */
/* If successful, return a pointer to a new instance of OGRAOIDataSource */
/* otherwise return NULL */
OGRDataSource* OGRAOIDriver::Open( const char *pszFileame, int bUpdate )
{
    OGRAOIDataSource *poDS = new OGRAOIDataSource();

    if( !poDS->Open( pszFileame, bUpdate ) )
    {
        delete poDS;
        return NULL;
    }
    else
    {
        return poDS;
    }
}

/* Test the capabilities of this driver */
/* we can't create datasets, but we can delete them */
int OGRAOIDriver::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return FALSE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}

/* Delete the data source - quite easy it is just one file */
int OGRAOIDriver::DeleteDataSource( const char *pszName )
{
    if( VSIUnlink( pszName ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to unlink %s failed.\n", pszName );
        return OGRERR_FAILURE;
    }
    else
        return OGRERR_NONE;
}
