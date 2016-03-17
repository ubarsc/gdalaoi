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

#include "aoilayer.h"
#include "aoiproj.h"
#include "math.h"

// This is my diagram of what an AOI file looks like (types with name in brackets):
// Eaoi_AreaOfInterest (AOInode)
//      Eaoi_AoiObjectType (AOIobject_X)   (one per aoi)
//      ...
//          Eaoi_AntAoiInfo (AOIantObject)
//              AntHeader_Eant (antInfo)
//                  Eprj_ProParameters (Projection)
//                  Eprj_MapInfo (Map_Info)
//                  ElementNode_Eant (ElementList)
//                      Element_2_Eant (AntElement_0)   (In here is 
//                                                          a polynomial that needs to be applied
//                                                          the the coords)
//                          Polgon2 (Polygon Info)      (for polygons)
//                          Polyline2 (Polyline Info)   (for lines)
//                          Ellipse2 (Ellipse Info)     (for ellipsis)
//                          Rectangle2 (Rectangle Info) (for rectangles)
//                          Point2 (Point Info)         (for points)
//                          
//                          Element_2_Eant (AntElement_1) (for grouped AOI's
//                                                          multiple Element_2_Eant can
//                                                          appear each with their own 
//                                                          poly and child shapes)
//                              Polgon2 (Polygon Info)
//                              ...

// Constructor
OGRAOILayer::OGRAOILayer( HFAEntry *pAOInode, const char *pszBasename )
{
    m_nNextFID = 0;
    m_pAOInode = pAOInode;
    m_pAOIObject = NULL;
    m_poSpatialRef = NULL;
    m_bEnd = FALSE;

    // Create the Feature Definition - GeometryCollection
    // and two text fields
    // Create layername from name of file and type
    m_poFeatureDefn = new OGRFeatureDefn( pszBasename );
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->SetGeomType( wkbGeometryCollection );
   
    OGRFieldDefn oFieldName( "Name", OFTString );
    m_poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldDescription( "Description", OFTString );
    m_poFeatureDefn->AddFieldDefn( &oFieldDescription );

    // get the number of steps for creating an ellipsis from the config
    const char *pszNSteps = CPLGetConfigOption("OGR_AOI_ELLIPSIS_STEPS", "36");
    m_nEllipsisSteps = atol(pszNSteps);
    if( m_nEllipsisSteps <= 0 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "OGR_AOI_ELLIPSIS_STEPS <= zero or invalid. Using 36");
        m_nEllipsisSteps = 36;
    }
}

// Destructor - release attached feature defn and spatial ref
OGRAOILayer::~OGRAOILayer()
{
    if( m_poFeatureDefn != NULL )
        m_poFeatureDefn->Release();
    if( m_poSpatialRef != NULL )
        m_poSpatialRef->Release();
}

// Return the spatial reference for this layer
// Construct it if this is the first time we have
// been asked, or return cached copy
OGRSpatialReference* OGRAOILayer::GetSpatialRef()
{
    if( m_poSpatialRef == NULL )
    {
        // note: already assigned refcount of 1 on creation
        m_poSpatialRef = CreateSpatialReference( m_pAOInode );
    }

    return m_poSpatialRef;
}

/* Returns the next Eaoi_AoiObjectType to use */
/* and updates the internal pointer */
HFAEntry* OGRAOILayer::GetNextAOIObject()
{
    /* so we don't start at the beginning again */
    if( m_bEnd )
        return NULL;

    if( m_pAOIObject == NULL )
    {
        /* At the start of the file */
        /* Get the first child of the AOInode */
        m_pAOIObject = m_pAOInode->GetChild();
    }
    else
    {
        /* otherwise move on one */
        m_pAOIObject = m_pAOIObject->GetNext();
    }
    
    /* if the current m_pAOIObject is not the right type */
    /* keep looping until it is, or we at end of file */
    while( ( m_pAOIObject != NULL ) &&
             !EQUAL(m_pAOIObject->GetType(),"Eaoi_AoiObjectType") )
    {
        m_pAOIObject = m_pAOIObject->GetNext();
    }

    /* we are at end of file, set our flag */
    if( m_pAOIObject == NULL )
        m_bEnd = TRUE;

    return m_pAOIObject;
}

// Given an AOIObject (from GetNextAOIObject)
// drill down and return the head Element_2_Eant for it
HFAEntry* OGRAOILayer::GetInfoFromAOIObject( HFAEntry *pAOIObject, 
                        const char **ppszName, const char **ppszDescription )
{
    HFAEntry *pInfo = NULL;

    HFAEntry *pAOIantObject = pAOIObject->GetNamedChild("AOIantObject");
    if( pAOIantObject != NULL )
    {
        HFAEntry *pantInfo = pAOIantObject->GetNamedChild("antInfo");
        if( pantInfo != NULL )
        {
            HFAEntry *pElementList = pantInfo->GetNamedChild("ElementList");
            if( pElementList != NULL )
            {
                HFAEntry *pElement = pElementList->GetChild();
                if( pElement != NULL )
                {
                    // grab the name and description
                    *ppszName = pElement->GetStringField("name");
                    *ppszDescription = pElement->GetStringField("description");

                    pInfo = pElement;
                }
            }
        }
    }

    return pInfo;
}

// Allow reading to begin at the start again
void OGRAOILayer::ResetReading()
{
    m_nNextFID = 0;
    m_pAOIObject = NULL;
    m_bEnd = FALSE;
}

// Given an HFAEntry that is a Polygon2, create a OGRGeometry for it
OGRGeometry * OGRAOILayer::HandlePolygon( HFAEntry *pInfo, Efga_Polynomial *pPoly )
{
    int nRows=0, nColumns;
    CPLErr err;
    // this is how you extract the dimensions of a basedata
    nColumns = pInfo->GetIntField("coords.coords[-2]", &err);
    if( err == CE_None )
        nRows = pInfo->GetIntField("coords.coords[-1]", &err);

    CPLString wkt("POLYGON((");
    if( ( err == CE_None ) && (nColumns > 0 ) && (nRows == 2 ) )
    {
        // Read in each coord and put it into the wkt
        double x = 0, y = 0;
        CPLString osBuffer;
        for( int nIndex = 0; ( err == CE_None ) && nIndex < (nColumns * nRows); nIndex+=2 )
        {
            osBuffer.Printf("coords.coords[%d]", nIndex );
            x = pInfo->GetDoubleField( osBuffer, &err );

            if( err == CE_None )
            {
                osBuffer.Printf("coords.coords[%d]", nIndex+1 );
                y = pInfo->GetDoubleField( osBuffer, &err );
            }
            if( err == CE_None )
            {
                // apply the transform - this handles rotation etc
                ApplyXformPolynomial( pPoly, &x, &y );
                osBuffer.Printf("%f %f,", x, y );
                wkt = wkt + osBuffer;
            }
        }
        // at end - close polygon
        if( err == CE_None )
        {
            x = pInfo->GetDoubleField( "coords.coords[0]", &err );
            if( err == CE_None )
                y = pInfo->GetDoubleField( "coords.coords[1]", &err );
            if( err == CE_None )
            {
                ApplyXformPolynomial( pPoly, &x, &y );
                osBuffer.Printf("%f %f))", x, y );
                wkt = wkt + osBuffer;
            }
        }
    }

    if( err == CE_None )
    {
        // Create OGRGeometry object
        const char *pszData = wkt.c_str();
        OGRGeometry *pGeom;
        if( OGRGeometryFactory::createFromWkt( const_cast<char **>(&pszData), GetSpatialRef(), &pGeom ) == OGRERR_NONE )
        {
            return pGeom;
        }
    }
    return NULL;
}

// Given an HFAEntry that is a Rectangle2, create a OGRGeometry for it
OGRGeometry *OGRAOILayer::HandleRectangle( HFAEntry *pInfo, Efga_Polynomial *pPoly )
{
    double dX, dY, dWidth, dHeight;
    double dTLX, dTLY, dBRX, dBRY, dTRX, dTRY, dBLX, dBLY;
    CPLErr err;

    dX = pInfo->GetDoubleField("center.x", &err);
    if( err == CE_None )
    {
        dY = pInfo->GetDoubleField("center.y", &err);
    }
    if( err == CE_None )
    {
        dWidth = pInfo->GetDoubleField("width", &err);
    }
    if( err == CE_None )
    {
        dHeight = pInfo->GetDoubleField("height", &err);
    }
    // orientation always seems to be 0 - handled by the pPoly

    if( err == CE_None )
    {
        // work out corners
        dTLX = dX - (dWidth / 2);
        dTLY = dY + (dHeight / 2);

        dTRX = dX + (dWidth / 2);
        dTRY = dTLY;

        dBRX = dX + (dWidth / 2);
        dBRY = dY - (dHeight / 2);

        dBLX = dTLX;
        dBLY = dBRY;

        // apply polynomial to each - handles rotation etc
        ApplyXformPolynomial( pPoly, &dTLX, &dTLY );
        ApplyXformPolynomial( pPoly, &dTRX, &dTRY );
        ApplyXformPolynomial( pPoly, &dBRX, &dBRY );
        ApplyXformPolynomial( pPoly, &dBLX, &dBLY );

        // create WKT
        CPLString sWKT;
        sWKT.Printf("POLYGON((%f %f,%f %f,%f %f,%f %f,%f %f))",
                    dTLX, dTLY, dTRX, dTRY, dBRX, dBRY, dBLX, dBLY, dTLX, dTLY );

        // Create OGRGeometry object
        const char *pszData = sWKT.c_str();
        OGRGeometry *pGeom;
        if( OGRGeometryFactory::createFromWkt( const_cast<char **>(&pszData), GetSpatialRef(), &pGeom ) == OGRERR_NONE )
        {
            return pGeom;
        }
    }

    return NULL;
}

// Given an HFAEntry that is a Ellipse2, create a OGRGeometry for it
// Note that since an ellipse type doesn't exist in OGR, we must turn
// it into a polygon with ELLIPSESTEPS points
OGRGeometry *OGRAOILayer::HandleEllipse( HFAEntry *pInfo, Efga_Polynomial *pPoly )
{
    double dCenterX, dCenterY, dSemiMajor, dSemiMinor, dAlpha, dX, dY;
    CPLErr err;

    dCenterX = pInfo->GetDoubleField("center.x", &err);
    if( err == CE_None )
    {
        dCenterY = pInfo->GetDoubleField("center.y", &err);
    }
    if( err == CE_None )
    {
        dSemiMajor = pInfo->GetDoubleField("semiMajorAxis", &err);
    }
    if( err == CE_None )
    {
        dSemiMinor = pInfo->GetDoubleField("semiMinorAxis", &err);
    }
    // orientation always seems to be 0 - handled by the pPoly

    if( err == CE_None )
    {
        // Do maths to get points and add to wkt
        CPLString sWKT("POLYGON((");
        CPLString osBuffer;
        for( dAlpha = 0; dAlpha < 360; dAlpha += (360.0/m_nEllipsisSteps) )
        {
            dX = dCenterX + (dSemiMajor * cos(dAlpha / 180.0 * M_PI));
            dY = dCenterY + (dSemiMinor * sin(dAlpha / 180.0 * M_PI));

            // Handles rotation etc
            ApplyXformPolynomial( pPoly, &dX, &dY );
            osBuffer.Printf("%f %f,", dX, dY );
            sWKT = sWKT + osBuffer;
        }
        // close poly
        dX = dCenterX + dSemiMajor;
        dY = dCenterY;

        ApplyXformPolynomial( pPoly, &dX, &dY );
        osBuffer.Printf("%f %f))", dX, dY );
        sWKT = sWKT + osBuffer;

        // Create OGRGeometry object
        const char *pszData = sWKT.c_str();
        OGRGeometry *pGeom;
        if( OGRGeometryFactory::createFromWkt( const_cast<char **>(&pszData), GetSpatialRef(), &pGeom ) == OGRERR_NONE )
        {
            return pGeom;
        }
    }

    return NULL;
}

// Given an HFAEntry that is a Rectangle2, create a OGRGeometry for it
OGRGeometry *OGRAOILayer::HandleLine( HFAEntry *pInfo, Efga_Polynomial *pPoly )
{
    int nRows=0, nColumns;
    CPLErr err;
    // this is how you extract the dimensions of a basedata
    nColumns = pInfo->GetIntField("coords.coords[-2]", &err);
    if( err == CE_None )
        nRows = pInfo->GetIntField("coords.coords[-1]", &err);

    CPLString wkt("LINESTRING(");
    // read in the points and add to wkt
    if( ( err == CE_None ) && (nColumns > 0 ) && (nRows == 2 ) )
    {
        double x = 0, y = 0;
        CPLString osBuffer;
        for( int nIndex = 0; ( err == CE_None ) && nIndex < (nColumns * nRows); nIndex+=2 )
        {
            osBuffer.Printf("coords.coords[%d]", nIndex );
            x = pInfo->GetDoubleField( osBuffer, &err );

            if( err == CE_None )
            {
                osBuffer.Printf("coords.coords[%d]", nIndex+1 );
                y = pInfo->GetDoubleField( osBuffer, &err );
            }
            if( err == CE_None )
            {
                // handles rotation etc
                ApplyXformPolynomial( pPoly, &x, &y );
                osBuffer.Printf("%f %f,", x, y );
                wkt = wkt + osBuffer;
            }
        }
        // get rid of trailing ','
        // and close
        wkt.erase(wkt.size() - 1);
        wkt = wkt + ")";
    }

    if( err == CE_None )
    {
        // create OGRGeometry object
        const char *pszData = wkt.c_str();
        OGRGeometry *pGeom;
        if( OGRGeometryFactory::createFromWkt( const_cast<char **>(&pszData), GetSpatialRef(), &pGeom ) == OGRERR_NONE )
        {
            return pGeom;
        }
    }

    return NULL;
}

// Given an HFAEntry that is a Point2, create a OGRGeometry for it
OGRGeometry *OGRAOILayer::HandlePoint( HFAEntry *pInfo, Efga_Polynomial *pPoly )
{
    int nRows=0, nColumns;
    CPLErr err;
    // this is how you extract the dimensions of a basedata
    nColumns = pInfo->GetIntField("coord.coords[-2]", &err);
    if( err == CE_None )
        nRows = pInfo->GetIntField("coord.coords[-1]", &err);

    CPLString sWKT;
    if( ( err == CE_None ) && (nColumns == 1 ) && (nRows == 2 ) )
    {
        double x = 0, y = 0;
        x = pInfo->GetDoubleField( "coord.coords[0]", &err );

        if( err == CE_None )
        {
            y = pInfo->GetDoubleField( "coord.coords[1]", &err );
        }
        if( err == CE_None )
        {
            // handle any movement etc
            ApplyXformPolynomial( pPoly, &x, &y );
            sWKT.Printf("POINT(%f %f)", x, y );
        }
    }

    if( err == CE_None )
    {
        // Create OGRGeometry class
        const char *pszData = sWKT.c_str();
        OGRGeometry *pGeom;
        if( OGRGeometryFactory::createFromWkt( const_cast<char **>(&pszData), GetSpatialRef(), &pGeom ) == OGRERR_NONE )
        {
            return pGeom;
        }
    }

    return NULL;
}

// This function is called recursively to add geometries to the
// OGRGeometryCollecion. 
// Is initially called with the head Element_2_Eant for the feature
void OGRAOILayer::HandleChildFeatures(HFAEntry *pNode, HFAEntry *pParent, OGRGeometryCollection *pCollection)
{
    // read the polynomial - will fail gracefully 
    // this this node doesn't have one
    Efga_Polynomial xformPoly;
    ReadXformPolynomial( pParent, &xformPoly );

    OGRGeometry *pGeom = NULL;
    // Note we just check the first part of the type string
    // (without the version). Hopefully later versions (if they exist)
    // have the same base fields.
    // Call the appropriate function for the shape
    if( EQUALN(pNode->GetType(),"Polygon",7) )
    {
        pGeom = HandlePolygon( pNode, &xformPoly );
    }
    else if( EQUALN(pNode->GetType(),"Rectangle",9) )
    {
        pGeom = HandleRectangle( pNode, &xformPoly );
    }
    else if( EQUALN(pNode->GetType(),"Ellipse",7) )
    {
        pGeom = HandleEllipse( pNode, &xformPoly );
    }
    else if( EQUALN(pNode->GetType(),"Polyline",8) )
    {
        pGeom = HandleLine( pNode, &xformPoly );
    }
    else if( EQUALN(pNode->GetType(),"Point",5) )
    {
        pGeom = HandlePoint( pNode, &xformPoly );
    }

    if( pGeom != NULL )
    {
        pCollection->addGeometryDirectly( pGeom );
    }

    // Now process any child entries recursively
    HFAEntry *pChild = pNode->GetChild();
    while( pChild != NULL )
    {
        HandleChildFeatures( pChild, pNode, pCollection );
        pChild = pChild->GetNext();
    }
}

// Return the next feature in the file. 
// Keeps looping until a node of the right type is found
OGRFeature *OGRAOILayer::GetNextFeature()
{
    while( TRUE )
    {
        HFAEntry *pAOIObject = GetNextAOIObject();
        if( pAOIObject == NULL ) // at end of file
            return NULL;

        const char *pszName = NULL, *pszDescription = NULL;

        // Create the geometry collection
        OGRGeometryCollection *pCollection = (OGRGeometryCollection*)
                OGRGeometryFactory::createGeometry(wkbGeometryCollection);

        // Get the head node for the feature
        HFAEntry *pInfo = GetInfoFromAOIObject( pAOIObject, &pszName, &pszDescription );
        // put all the child geometries into the collection
        if( pInfo != NULL )
        {
            HandleChildFeatures( pInfo, pInfo, pCollection );
        }

        if( pCollection->getNumGeometries() > 0 )
        {
            // Create the feature
            OGRFeature *poFeature = new OGRFeature( m_poFeatureDefn );
            poFeature->SetGeometryDirectly( pCollection );
            poFeature->SetField( 0, pszName );
            poFeature->SetField( 1, pszDescription );
            poFeature->SetFID( m_nNextFID++ );
            // do spatial and attribute test
            if( (m_poFilterGeom == NULL
                 || FilterGeometry( poFeature->GetGeometryRef() ) )
                && (m_poAttrQuery == NULL
                    || m_poAttrQuery->Evaluate( poFeature )) )
                return poFeature;
            else
                delete poFeature;
        }
        else
        {
            // no geometries added - don't add feature
            // is this the right thing to do?
            OGRGeometryFactory::destroyGeometry( pCollection );
        }

    }

    return NULL;
}


