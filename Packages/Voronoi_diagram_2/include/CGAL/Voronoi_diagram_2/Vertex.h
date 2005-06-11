// Copyright (c) 2005 Foundation for Research and Technology-Hellas (Greece).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org); you may redistribute it under
// the terms of the Q Public License version 1.0.
// See the file LICENSE.QPL distributed with CGAL.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $Source$
// $Revision$ $Date$
// $Name$
//
// Author(s)     : Menelaos Karavelas <mkaravel@tem.uoc.gr>

#ifndef CGAL_VORONOI_DIAGRAM_2_VERTEX_H
#define CGAL_VORONOI_DIAGRAM_2_VERTEX_H 1

#include <CGAL/Voronoi_diagram_adaptor_2/basic.h>
#include <CGAL/Voronoi_diagram_adaptor_2/Finder_classes.h>


CGAL_BEGIN_NAMESPACE

CGAL_VORONOI_DIAGRAM_2_BEGIN_NAMESPACE

template<class VDA>
class Vertex
{
 private:
  typedef Vertex<VDA>                     Self;
  typedef Triangulation_cw_ccw_2          CW_CCW_2;

  typedef typename VDA::Dual_face_handle  Dual_face_handle;

 private:
  Dual_face_handle find_valid_vertex(const Dual_face_handle& f) const
  {
    return Find_valid_vertex<VDA>()(vda_, f);
  }

 public:
  typedef typename VDA::Halfedge          Halfedge;
  typedef typename VDA::Halfedge_handle   Halfedge_handle;
  typedef typename VDA::Face              Face;
  typedef typename VDA::Face_handle       Face_handle;
  typedef typename VDA::Vertex_handle     Vertex_handle;
  typedef typename VDA::size_type         size_type;

  typedef typename VDA::Halfedge_around_vertex_circulator
  Halfedge_around_vertex_circulator;

  typedef typename VDA::Voronoi_traits::Point_2           Point_2;
  typedef typename VDA::Voronoi_traits::Voronoi_vertex_2  Voronoi_vertex_2;

 public:
  Vertex(const VDA* vda = NULL) : vda_(vda) {}
  Vertex(const VDA* vda, Dual_face_handle f) : vda_(vda), f_(f) {
    CGAL_precondition( !vda_->dual().is_infinite(f_) );
  }

  Halfedge_handle halfedge() const {
    Dual_face_handle fvalid = find_valid_vertex(f_);
    for (int i = 0; i < 3; i++) {
      int ccw_i = CW_CCW_2::ccw(i);
      
      // if I want to return also infinite edges replace the test in
      // the if statement by the following test (i.e., should omit the
      // testing for infinity):
      //           !vda_->edge_tester()(vda_->dual(), fvalid, i)
      if ( !vda_->edge_tester()(vda_->dual(), fvalid, i) &&
	   !vda_->dual().is_infinite(fvalid, i) ) {
	if ( vda_->face_tester()(vda_->dual(), fvalid->vertex(ccw_i)) ) {
	  Dual_face_handle fopp;
	  int iopp, i_mirror = vda_->dual().tds().mirror_index(fvalid, i);

	  Find_opposite_halfedge<VDA>()(vda_,
					fvalid->neighbor(i),
					i_mirror,
					fopp, iopp);
#if !defined(CGAL_NO_ASSERTIONS) && !defined(NDEBUG)
	  Halfedge h(vda_, fopp, iopp);
	  Vertex_handle v_this(*this);
	  CGAL_assertion( h.has_target() && h.target() == v_this );
#endif
	  return Halfedge_handle( Halfedge(vda_, fopp, iopp) );
	} else {
#if !defined(CGAL_NO_ASSERTIONS) && !defined(NDEBUG)
	  Halfedge h(vda_, fvalid, i);
	  Vertex_handle v_this(*this);
	  CGAL_assertion( h.has_target() && h.target() == v_this );
#endif
	  return Halfedge_handle( Halfedge(vda_, fvalid, i) );
	}
      }
    }

#if !defined(CGAL_NO_ASSERTIONS) && !defined(NDEBUG)
    bool this_line_should_never_have_been_reached = false;
    CGAL_assertion(this_line_should_never_have_been_reached);
#endif
    return Halfedge_handle();
  }

  Halfedge_around_vertex_circulator incident_halfedges() const {
    CGAL_assertion( halfedge()->has_target() &&
		    halfedge()->target() == Vertex_handle(*this) );
    return Halfedge_around_vertex_circulator( *halfedge() );
  }

  bool is_incident_edge(const Halfedge_handle& he) const {
    bool res = false;
    if ( he->has_target() ) {
      res = res || he->target() == Vertex_handle(*this);
    }
    if ( he->has_source() ) {
      res = res || he->source() == Vertex_handle(*this);
    }
    return res;
  }

  bool is_incident_face(const Face_handle& f) const {
    Halfedge_around_vertex_circulator hc = incident_halfedges();
    Halfedge_around_vertex_circulator hc_start = hc;
    do {
      if ( hc->face() == f ) { return true; }
      ++hc;
    } while ( hc != hc_start );
    return false;
  }

  size_type degree() const {
    Halfedge_around_vertex_circulator hc = incident_halfedges();
    Halfedge_around_vertex_circulator hc_start = hc;
    size_type deg = 0;
    do {
      hc++;
      deg++;
    } while ( hc != hc_start );
    return deg;
  }

  bool operator==(const Self& other) const {
    if ( vda_ == NULL ) { return other.vda_ == NULL; }
    if ( other.vda_ == NULL ) { return vda_ == NULL; }
    return ( vda_ == other.vda_ && f_ == other.f_ );
  }

  bool operator!=(const Self& other) const {
    return !((*this) == other);
  }

  const Dual_face_handle& dual_face() const { return f_; }

  Point_2 point() const {
    Dual_face_handle fvalid = find_valid_vertex(f_);
    CGAL_assertion( !vda_->dual().is_infinite(fvalid) );

    return VDA::Voronoi_traits::make_vertex(fvalid->vertex(0),
					    fvalid->vertex(1),
					    fvalid->vertex(2));
  }

  bool is_valid() const {
    if ( vda_ == NULL ) { return true; }

    bool valid = !vda_->dual().is_infinite(f_);

    valid = valid && is_incident_edge( halfedge() );

    Vertex_handle v_this(*this);

    if ( halfedge()->has_source() ) {
      valid = valid && halfedge()->opposite()->source() == v_this;
    }
    if ( halfedge()->has_target() ) {
      valid = valid && halfedge()->target() == v_this;
    }

    Halfedge_around_vertex_circulator hc = incident_halfedges();
    Halfedge_around_vertex_circulator hc_start = hc;
    do {
      valid = valid && hc->has_target() && hc->target() == v_this;
      ++hc;
    } while ( hc != hc_start );

    Halfedge_handle hhc = *incident_halfedges();
    Halfedge_handle hhc_start = hhc;
    do {
      valid = valid && hhc->has_target() && hhc->target() == v_this;
      hhc = hhc->next()->twin();
    } while ( hhc != hhc_start );

    return valid;
  }

 private:
  const VDA* vda_;
  Dual_face_handle f_;
};


CGAL_VORONOI_DIAGRAM_2_END_NAMESPACE

CGAL_END_NAMESPACE


#endif // CGAL_VORONOI_DIAGRAM_2_VERTEX_H
