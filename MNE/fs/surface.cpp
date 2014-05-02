//=============================================================================================================
/**
* @file     surface.cpp
* @author   Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>;
* @version  1.0
* @date     March, 2013
*
* @section  LICENSE
*
* Copyright (C) 2013, Christoph Dinh and Matti Hamalainen. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that
* the following conditions are met:
*     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
*       following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*       the following disclaimer in the documentation and/or other materials provided with the distribution.
*     * Neither the name of the Massachusetts General Hospital nor the names of its contributors may be used
*       to endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MASSACHUSETTS GENERAL HOSPITAL BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*
* @brief    Implementation of the Surface class.
*
*/

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "surface.h"
#include <utils/ioutils.h>

#include <iostream>


//*************************************************************************************************************
//=============================================================================================================
// Qt INCLUDES
//=============================================================================================================

#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QDebug>


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace UTILSLIB;
using namespace FSLIB;


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

Surface::Surface()
: m_fileName("")
, hemi(-1)
, surf("")
{
}


//*************************************************************************************************************

Surface::Surface(const QString& p_sFileName)
: m_fileName(p_sFileName)
, hemi(-1)
{
    Surface::read(p_sFileName, *this);
}


//*************************************************************************************************************

Surface::~Surface()
{
}


//*************************************************************************************************************

void Surface::clear()
{
    m_fileName.clear();
    hemi = -1;
    surf.clear();
    rr.resize(0,3);
    tris.resize(0,3);
    nn.resize(0,3);
    curv.resize(0);
}


//*************************************************************************************************************

MatrixX3f Surface::compute_normals(const MatrixX3f& rr, const MatrixX3i& tris)
{
    printf("\tcomputing normals\n");
    // first, compute triangle normals
    MatrixX3f r1(tris.rows(),3); MatrixX3f r2(tris.rows(),3); MatrixX3f r3(tris.rows(),3);

    for(qint32 i = 0; i < tris.rows(); ++i)
    {
        r1.row(i) = rr.row(tris(i, 0));
        r2.row(i) = rr.row(tris(i, 1));
        r3.row(i) = rr.row(tris(i, 2));
    }

    MatrixX3f x = r2 - r1;
    MatrixX3f y = r3 - r1;
    MatrixX3f tri_nn(x.rows(),y.cols());
    tri_nn.col(0) = x.col(1).cwiseProduct(y.col(2)) - x.col(2).cwiseProduct(y.col(1));
    tri_nn.col(1) = x.col(2).cwiseProduct(y.col(0)) - x.col(0).cwiseProduct(y.col(2));
    tri_nn.col(2) = x.col(0).cwiseProduct(y.col(1)) - x.col(1).cwiseProduct(y.col(0));

    //   Triangle normals and areas
    MatrixX3f tmp = tri_nn.cwiseProduct(tri_nn);
    VectorXf normSize = tmp.rowwise().sum();
    normSize = normSize.cwiseSqrt();

    for(qint32 i = 0; i < normSize.size(); ++i)
        if(normSize(i) != 0)
            tri_nn.row(i) /= normSize(i);

    MatrixX3f nn = MatrixX3f::Zero(rr.rows(), 3);

    for(qint32 p = 0; p < tris.rows(); ++p)
    {
        Vector3i verts = tris.row(p);
        for(qint32 j = 0; j < verts.size(); ++j)
            nn.row(verts(j)) = tri_nn.row(p);
    }

    tmp = nn.cwiseProduct(nn);
    normSize = tmp.rowwise().sum();
    normSize = normSize.cwiseSqrt();

    for(qint32 i = 0; i < normSize.size(); ++i)
        if(normSize(i) != 0)
            nn.row(i) /= normSize(i);

    return nn;
}


//*************************************************************************************************************

bool Surface::read(const QString &p_sFileName, Surface &p_Surface, bool p_bLoadCurvature)
{
    p_Surface.clear();

    printf("Reading surface...\n");
    QFile t_File(p_sFileName);

    if (!t_File.open(QIODevice::ReadOnly))
    {
        printf("\tError: Couldn't open the surface file\n");
        return false;
    }

    QDataStream t_DataStream(&t_File);
    t_DataStream.setByteOrder(QDataStream::BigEndian);

    //
    //   Magic numbers to identify QUAD and TRIANGLE files
    //
    //   QUAD_FILE_MAGIC_NUMBER =  (-1 & 0x00ffffff) ;
    //   NEW_QUAD_FILE_MAGIC_NUMBER =  (-3 & 0x00ffffff) ;
    //
    qint32 NEW_QUAD_FILE_MAGIC_NUMBER =  16777213;
    qint32 TRIANGLE_FILE_MAGIC_NUMBER =  16777214;
    qint32 QUAD_FILE_MAGIC_NUMBER     =  16777215;

    qint32 magic = IOUtils::fread3(t_DataStream);

    qint32 nvert, nface;
    MatrixXf verts;
    MatrixXi faces;

    if(magic == QUAD_FILE_MAGIC_NUMBER || magic == NEW_QUAD_FILE_MAGIC_NUMBER)
    {
        qint32 nvert = IOUtils::fread3(t_DataStream);
        qint32 nquad = IOUtils::fread3(t_DataStream);
        if(magic == QUAD_FILE_MAGIC_NUMBER)
            printf("\t%s is a quad file (nvert = %d nquad = %d)\n", p_sFileName.toLatin1().constData(),nvert,nquad);
        else
            printf("\t%s is a new quad file (nvert = %d nquad = %d)\n", p_sFileName.toLatin1().constData(),nvert,nquad);

        //vertices
        verts.resize(nvert, 3);
        if(magic == QUAD_FILE_MAGIC_NUMBER)
        {
            qint16 iVal;
            for(qint32 i = 0; i < nvert; ++i)
            {
                for(qint32 j = 0; j < 3; ++j)
                {
                    t_DataStream >> iVal;
                    IOUtils::swap_short(iVal);
                    verts(i,j) = ((float)iVal) / 100;
                }
            }
        }
        else
        {
            t_DataStream.readRawData((char *)verts.data(), nvert*3*sizeof(float));
            for(qint32 i = 0; i < nvert; ++i)
                for(qint32 j = 0; j < 3; ++j)
                    IOUtils::swap_floatp(&verts(i,j));
        }

        MatrixXi quads = IOUtils::fread3_many(t_DataStream, nquad*4);
        MatrixXi quads_new(4, nquad);
        qint32 count = 0;
        for(qint32 j = 0; j < quads.cols(); ++j)
        {
            for(qint32 i = 0; i < quads.rows(); ++i)
            {
                quads_new(i,j) = quads(count, 0);
                ++count;
            }
        }
        quads = quads_new.transpose();
        //
        //  Face splitting follows
        //
        faces = MatrixXi::Zero(2*nquad,3);
        nface = 0;
        for(qint32 k = 0; k < nquad; ++k)
        {
            RowVectorXi quad = quads.row(k);
            if ((quad[0] % 2) == 0)
            {
                faces(nface,0) = quad[0];
                faces(nface,1) = quad[1];
                faces(nface,2) = quad[3];
                ++nface;

                faces(nface,0) = quad[2];
                faces(nface,1) = quad[3];
                faces(nface,2) = quad[1];
                ++nface;
            }
            else
            {
                faces(nface,0) = quad(0);
                faces(nface,1) = quad(1);
                faces(nface,2) = quad(2);
                ++nface;

                faces(nface,0) = quad(0);
                faces(nface,1) = quad(2);
                faces(nface,2) = quad(3);
                ++nface;
            }
        }
    }
    else if(magic == TRIANGLE_FILE_MAGIC_NUMBER)
    {
        QString s = t_File.readLine();
        t_File.readLine();

        t_DataStream >> nvert;
        t_DataStream >> nface;
        IOUtils::swap_int(nvert);
        IOUtils::swap_int(nface);

        printf("\t%s is a triangle file (nvert = %d ntri = %d)\n", p_sFileName.toLatin1().constData(), nvert, nface);
        printf("\t%s", s.toLatin1().constData());

        //vertices
        verts.resize(3, nvert);
        t_DataStream.readRawData((char *)verts.data(), nvert*3*sizeof(float));
        for(qint32 i = 0; i < 3; ++i)
            for(qint32 j = 0; j < nvert; ++j)
                IOUtils::swap_floatp(&verts(i,j));

        //faces
        faces.resize(nface, 3);
        qint32 iVal;
        for(qint32 i = 0; i < nface; ++i)
        {
            for(qint32 j = 0; j < 3; ++j)
            {
                t_DataStream >> iVal;
                IOUtils::swap_int(iVal);
                faces(i,j) = iVal;
            }
        }
    }
    else
    {
        qWarning("Bad magic number (%d) in surface file %s",magic,p_sFileName.toLatin1().constData());
        return false;
    }

    verts.transposeInPlace();
    verts.array() *= 0.001f;

    p_Surface.rr = verts.block(0,0,verts.rows(),3);
    p_Surface.tris = faces.block(0,0,faces.rows(),3);

    p_Surface.nn = compute_normals(p_Surface.rr, p_Surface.tris);

    // hemi info
    if(t_File.fileName().contains("lh."))
        p_Surface.hemi = 0;
    else if(t_File.fileName().contains("rh."))
        p_Surface.hemi = 1;
    else
    {
        p_Surface.hemi = -1;
        return false;
    }

    //Surface loaded
    p_Surface.surf = t_File.fileName().right(4);

    t_File.close();
    printf("\tRead a surface with %d vertices from %s\n",nvert,p_sFileName.toLatin1().constData());

    return true;
}


//*************************************************************************************************************

VectorXf Surface::read_curv(const QString &p_sFileName)
{
    VectorXf curv;

    printf("Reading curvature...\n");
    QFile t_File(p_sFileName);

    if (!t_File.open(QIODevice::ReadOnly))
    {
        printf("\tError: Couldn't open the curvature file\n");
        return curv;
    }

    QDataStream t_DataStream(&t_File);
    t_DataStream.setByteOrder(QDataStream::BigEndian);

    qint32 vnum = IOUtils::fread3(t_DataStream);
    qint32 NEW_VERSION_MAGIC_NUMBER = 16777215;

    if(vnum == NEW_VERSION_MAGIC_NUMBER)
    {
        vnum = IOUtils::fread3(t_DataStream);
        qint32 fnum = IOUtils::fread3(t_DataStream);
        qint32 vals_per_vertex = IOUtils::fread3(t_DataStream);

        curv.resize(vnum, 1);
        t_DataStream.readRawData((char *)curv.data(), vnum*sizeof(float));
        for(qint32 i = 0; i < vnum; ++i)
           IOUtils::swap_floatp(&curv(i));
    }
    else
    {
        qint32 fnum = IOUtils::fread3(t_DataStream);
        qint16 iVal;
        curv.resize(vnum, 1);
        for(qint32 i = 0; i < vnum; ++i)
        {
            t_DataStream >> iVal;
            IOUtils::swap_short(iVal);
            curv(i) = ((float)iVal) / 100;
        }
    }
    t_File.close();

    return curv;
}
