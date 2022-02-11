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

#ifndef AOILAYER_H
#define AOILAYER_H

#include <ogrsf_frmts.h>
#include "hfa_p.h"

// Classes for representing layers in an AOI file
// We have 3 layers - one for Polygons, one for lines
// and one for points. 
// These are represented by different classes all
// derived from OGRAOILayer which has common functionality
class OGRAOILayer : public OGRLayer
{
protected:
    OGRFeatureDefn         *m_poFeatureDefn;
    std::unique_ptr<OGRSpatialReference>    m_poSpatialRef;

    HFAEntry               *m_pAOInode;
    HFAEntry               *m_pAOIObject; // pointer to current Eaoi_AoiObjectType

    int                     m_nNextFID;
    int                     m_bEnd;

    HFAEntry*           GetNextAOIObject();
    HFAEntry*           GetInfoFromAOIObject(HFAEntry *pAOIObject, 
                            const char **ppszName, const char **ppszDescription );

    void HandleChildFeatures(HFAEntry *pNode, HFAEntry *pParent, OGRGeometryCollection *pCollection);
    OGRGeometry *       HandlePolygon( HFAEntry *pInfo, Efga_Polynomial *pPoly );
    OGRGeometry *       HandleRectangle( HFAEntry *pInfo, Efga_Polynomial *pPoly );
    OGRGeometry *       HandleEllipse( HFAEntry *pInfo, Efga_Polynomial *pPoly );
    OGRGeometry *       HandleLine( HFAEntry *pInfo, Efga_Polynomial *pPoly );
    OGRGeometry *       HandlePoint( HFAEntry *pInfo, Efga_Polynomial *pPoly );

    int m_nEllipsisSteps;

  public:
    OGRAOILayer( HFAEntry *pAOInode, const char *pszBasename );
   ~OGRAOILayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return m_poFeatureDefn; }
    OGRSpatialReference * GetSpatialRef();

    int                 TestCapability( const char * ) { return FALSE; }
};

#endif // AOILAYER_H
