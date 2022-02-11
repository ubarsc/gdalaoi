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

#ifndef AOIPROJ_H
#define AOIPROJ_H

// Routines for dealing with projections and
// transform polynomials
// Adapted from HFA driver
std::unique_ptr<OGRSpatialReference> CreateSpatialReference( HFAEntry* pAOInode );
const Eprj_ProParameters *AOIGetProParameters( HFAEntry *poAntNode );
const Eprj_Datum *AOIGetDatum( HFAEntry *poAntNode );
const Eprj_MapInfo *AOIGetMapInfo( HFAEntry *poAntNode );

void ApplyXformPolynomial( Efga_Polynomial *pPoly, double *pdfX, double *pdfY );
void ReadXformPolynomial( HFAEntry *pElement, Efga_Polynomial *pPoly );

#endif // AOIPROJ_H
