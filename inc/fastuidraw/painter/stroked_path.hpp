/*!
 * \file stroked_path.hpp
 * \brief file stroked_path.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */


#pragma once

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/opaque_reference_counted.hpp>
#include <fastuidraw/painter/painter_attribute_data.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw  {

///@cond
class TessellatedPath;
class Path;
class PainterAttribute;
///@endcond

/*!\addtogroup Paths
  @{
 */

/*!
  \brief
  A StrokedPath represents the data needed to draw a path stroked.
  It contains -all- the data needed to stroke a path regardless of
  stroking style. In particular, for a given TessellatedPath,
  one only needs to construct a StrokedPath <i>once</i> regardless
  of how one strokes the original path for drawing.
 */
class StrokedPath:
    public reference_counted<StrokedPath>::non_concurrent
{
public:
  /*!
    \brief
    Enumeration for specifing how to compute
    \ref point::offset_vector()).
   */
  enum offset_type_t
    {
      /*!
        The point is for an edge of the path,
        point signifies the start of a sub-edge
        (quad) of drawing an edge.
       */
      offset_start_sub_edge,

      /*!
        The point is for an edge of the path,
        point signifies the end of a sub-edge
        (quad) of drawing an edge.
       */
      offset_end_sub_edge,

      /*!
        The point is at a position that has the
        same value as point on an edge but the
        point itself is part of a cap or join.
       */
      offset_shared_with_edge,

      /*!
        The point is for a boundary point of a rounded join of the path
       */
      offset_rounded_join,

      /*!
        Point type for miter-clip join point whose position
        depends on the miter-limit.
       */
      offset_miter_clip_join,

      /*!
        Point type for miter-clip join point whose position
        depends on the miter-limit where the lambda of the
        computation is negatted.
       */
      offset_miter_clip_join_lambda_negated,

      /*!
        Point type for miter-bevel join point whose position
        depends on the miter-limit.
       */
      offset_miter_bevel_join,

      /*!
        Point type for miter join whose position depends on
        the miter-limit.
       */
      offset_miter_join,

      /*!
        The point is for a boundary point of a rounded cap of the path
       */
      offset_rounded_cap,

      /*!
        The point is for a boundary point of a square cap of the path
       */
      offset_square_cap,

      /*!
        The point is from \ref adjustable_caps() point set.
        It is for a point for a cap at the start of a contour.
        These points are for dashed stroking with caps; they
        contain data to allow one from a vertex shader to extend
        or shrink the cap area correctly to implement dashed
        stroking.
       */
      offset_adjustable_cap_contour_start,

      /*!
        The point is from \ref adjustable_caps() point set.
        It is for a point for a cap at the end of a contour.
        These points are for dashed stroking with caps; they
        contain data to allow one from a vertex shader to extend
        or shrink the cap area correctly to implement dashed
        stroking.
       */
      offset_adjustable_cap_contour_end,

      /*!
        Number different point types with respect to rendering
       */
      number_offset_types
    };

  /*!
    \brief
    Enumeration encoding of bits of point::m_packed_data
    common to all offset types.
   */
  enum packed_data_bit_layout_common_t
    {
      /*!
        Bit0 for holding the offset_type() value
        of the point.
       */
      offset_type_bit0 = 0,

      /*!
        number of bits needed to hold the
        offset_type() value of the point.
       */
      offset_type_num_bits = 4,

      /*!
        Bit for holding boundary() value
        of the point
       */
      boundary_bit = offset_type_bit0 + offset_type_num_bits,

      /*!
        Bit0 for holding the depth() value
        of the point
       */
      depth_bit0,

      /*!
        number of bits needed to hold the
        depth() value of the point.
       */
      depth_num_bits = 20,

      /*!
        Bit to indicate point is from a join. For these
        points, during dashed stroking, Painter does the
        check if a join should be drawn, as such when the
        bit is up encountered in a shader, the computation
        to check that it is drawn from dashing can be
        skipped and assume that fragments from such points
        are covered by the dash pattern.
       */
      join_bit = depth_bit0 + depth_num_bits,

      /*!
        Number of bits used on common packed data
       */
      number_common_bits,
    };

  /*!
    \brief
    Enumeration encoding of bits of point::m_packed_data
    for those with offset type \ref offset_rounded_join
   */
  enum packed_data_bit_layout_rounded_join_t
    {
      /*!
        Bit for holding the the sign of
        the y-coordinate of the normal 0
        for the offset_type() \ref
        offset_rounded_join.
       */
      normal0_y_sign_bit = number_common_bits,

      /*!
        Bit for holding the the sign of
        the y-coordinate of the normal 1
        for the offset_type() \ref
        offset_rounded_join.
       */
      normal1_y_sign_bit,

      /*!
        Bit for holding the the sign of
        sin() value for the offset_type()
        \ref offset_rounded_join.
       */
      sin_sign_bit,
    };

  /*!
    \brief
    Enumeration encoding of bits of point::m_packed_data for
    those with offset type \ref offset_adjustable_cap_contour_end
    or \ref offset_adjustable_cap_contour_start.
   */
  enum packed_data_bit_adjustable_cap_t
    {
      /*!
        The bit is up if the point is for end of point
        of a cap (i.e. the side to be extended to make
        sure the entire cap near the end of edge is drawn).
       */
      adjustable_cap_ending_bit = number_common_bits,
    };

  /*!
    \brief
    Enumeration encoding of bits of point::m_packed_data
    for those with offset type \ref offset_start_sub_edge
    or \ref offset_end_sub_edge.
   */
  enum packed_data_bit_sub_edge_t
    {
      /*!
        The bit is up if the point is for the
        geometry of a bevel between two sub-edges.
       */
      bevel_edge_bit = number_common_bits,
    };

  /*!
    \brief
    Enumeration holding bit masks generated from
    values in \ref packed_data_bit_layout_common_t,
    \ref packed_data_bit_layout_rounded_join_t,
    \ref packed_data_bit_adjustable_cap_t and \ref
    packed_data_bit_sub_edge_t.
   */
  enum packed_data_bit_masks_t
    {
      /*!
        Mask generated for \ref offset_type_bit0 and
        \ref offset_type_num_bits
       */
      offset_type_mask = FASTUIDRAW_MASK(offset_type_bit0, offset_type_num_bits),

      /*!
        Mask generated for \ref normal0_y_sign_bit
       */
      normal0_y_sign_mask = FASTUIDRAW_MASK(normal0_y_sign_bit, 1),

      /*!
        Mask generated for \ref normal1_y_sign_bit
       */
      normal1_y_sign_mask = FASTUIDRAW_MASK(normal1_y_sign_bit, 1),

      /*!
        Mask generated for \ref sin_sign_bit sin_sign_bit
       */
      sin_sign_mask = FASTUIDRAW_MASK(sin_sign_bit, 1),

      /*!
        Mask generated for \ref boundary_bit
       */
      boundary_mask = FASTUIDRAW_MASK(boundary_bit, 1),

      /*!
        Mask generated for \ref join_bit
       */
      join_mask = FASTUIDRAW_MASK(join_bit, 1),

      /*!
        Mask generated for \ref adjustable_cap_ending_bit
       */
      adjustable_cap_ending_mask = FASTUIDRAW_MASK(adjustable_cap_ending_bit, 1),

      /*!
        Mask generated for \ref bevel_edge_bit
       */
      bevel_edge_mask = FASTUIDRAW_MASK(bevel_edge_bit, 1),

      /*!
        Mask generated for \ref depth_bit0 and \ref depth_num_bits
       */
      depth_mask = FASTUIDRAW_MASK(depth_bit0, depth_num_bits),
    };

  /*!
    \brief
    A point holds the data for a point of stroking.
    The data is so that changing the stroking width
    or miter limit does not change the stroking data.
   */
  class point
  {
  public:
    /*!
      The base position of a point, taken directly from
      the TessellatedPath from which the
      StrokedPath is constructed.
     */
    vec2 m_position;

    /*!
      Gives the offset vector used to help compute
      the point offset vector.
    */
    vec2 m_pre_offset;

    /*!
      Provides an auxilary offset data
     */
    vec2 m_auxilary_offset;

    /*!
      Gives the distance of the point from the start
      of the -edge- on which the point resides.
     */
    float m_distance_from_edge_start;

    /*!
      Gives the distance of the point from the start
      of the -contour- on which the point resides.
     */
    float m_distance_from_contour_start;

    /*!
      Gives the length of the edge on which the
      point lies. This value is the same for all
      points along a fixed edge.
     */
    float m_edge_length;

    /*!
      Gives the length of the contour open on which
      the point lies. This value is the same for all
      points along a fixed contour.
     */
    float m_open_contour_length;

    /*!
      Gives the length of the contour closed on which
      the point lies. This value is the same for all
      points along a fixed contour.
     */
    float m_closed_contour_length;

    /*!
      Bit field with data packed as according to
      \ref packed_data_bit_layout_common_t, \ref
      packed_data_bit_layout_rounded_join_t, \ref
      packed_data_bit_adjustable_cap_t and \ref
      packed_data_bit_sub_edge_t. See also,
      \ref packed_data_bit_masks_t for bit masks
      generated.
     */
    uint32_t m_packed_data;

    /*!
      Provides the point type from a value of \ref m_packed_data.
      The return value is one of the enumerations of
      \ref offset_type_t.
      \param packed_data_value value suitable for \ref m_packed_data
     */
    static
    enum offset_type_t
    offset_type(uint32_t packed_data_value)
    {
      uint32_t v;
      v = unpack_bits(offset_type_bit0, offset_type_num_bits, packed_data_value);
      return static_cast<enum offset_type_t>(v);
    }

    /*!
      Provides the point type for the point. The return value
      is one of the enumerations of \ref offset_type_t.
     */
    enum offset_type_t
    offset_type(void) const
    {
      return offset_type(m_packed_data);
    }
    
    /*!
      When stroking the data, the depth test to only
      pass when the depth value is -strictly- larger
      so that a fixed pixel is not stroked twice by
      a single path. The value returned by depth() is
      a relative z-value for a vertex. The points
      drawn first have the largest z-values.
     */
    uint32_t
    depth(void) const
    {
      return unpack_bits(depth_bit0, depth_num_bits, m_packed_data);
    }

    /*!
      Has value 0 or 1. If the value is 0, then
      the point is on the path. If the value has
      absolute value 1, then indicates a point that
      is on the boundary of the stroked path. The triangles
      produced from stroking are so that when
      on_boundary() is interpolated across the triangle
      the center of stroking the value is 0 and the
      value has value 1 on the boundary.
     */
    int
    on_boundary(void) const
    {
      return unpack_bits(boundary_bit, 1u, m_packed_data);
    }

    /*!
      Computes the offset vector for a point. The final position
      of a point when stroking with a width of W is given by
      \code
      m_position + 0.5f * W * offset_vector().
      \endcode
      The computation for offset_vector() is as follows.
      - For those with offset_type() being \ref offset_start_sub_edge,
        \ref offset_end_sub_edge and \ref offset_shared_with_edge,
        the offset is given by
        \code
        m_pre_offset
        \endcode
        In addition, for these offset types, \ref m_auxilary_offset
        holds the the delta vector to the point on edge with
        which the point makes a quad.
      - For those with offset_type() being \ref offset_rounded_join,
        the value is given by the following code
        \code
        vec2 cs;

        cs.x() = m_auxilary_offset.y();
        cs.y() = sqrt(1.0 - cs.x() * cs.x());

        if(m_packed_data & sin_sign_mask)
          cs.y() = -cs.y();

        offset = cs
        \endcode
        In addition, the source data for join is encoded as follows:
        \code
        float t;
        vec2 n0, n1;

        t = m_auxilary_offset.x();
        n0.x() = m_pre_offset.x();
        n1.x() = m_pre_offset.y();

        n0.y() = sqrt(1.0 - n0.x() * n0.x());
        n1.y() = sqrt(1.0 - n1.x() * n1.x());

        if(m_packed_data & normal0_y_sign_mask)
          n0.y() = -n0.y();

        if(m_packed_data & normal1_y_sign_mask)
          n1.y() = -n1.y();
        \endcode
        The vector n0 represents the normal of the path going into the join,
        the vector n1 represents the normal of the path going out of the join
        and t represents how much to interpolate from n0 to n1.
      - For those with offset_type() being \ref offset_miter_clip_join
        or \ref offset_miter_clip_join_lambda_negated
        the value is given by the following code
        \code
        vec2 n0(m_pre_offset), Jn0(n0.y(), -n0.x());
        vec2 n1(m_auxilary_offset), Jn1(n1.y(), -n1.x());
        float r, det, lambda;
        det = dot(Jn1, n0);
        lambda = -t_sign(det);
        r = (det != 0.0) ? (dot(n0, n1) - 1.0) / det : 0.0;
        if(offset_type() == offset_miter_clip_join_lambda_negated)
          {
            lambda = -lambda;
          }
        //result:
        offset = lambda * (n + r * v);
        \endcode
      - For those with offset_type() being \ref offset_miter_join,
        or \ref offset_miter_bevel_join the value is given by the
        following code:
        \code
        vec2 n0(m_pre_offset), Jn0(n0.y(), -n0.x());
        vec2 n1(m_auxilary_offset), Jn1(n1.y(), -n1.x());
        float lambda, r, d;
        lambda = t_sign(dot(Jn0, n1));
        r = lambda / (1.0 + dot(n0, n1));
        offset = r * (n0 + n1);
        \endcode
      - For those with offset_type() being \ref offset_rounded_cap,
        the value is given by the following code
        \code
        vec2 n(m_pre_offset), v(n.y(), -n.x());
        offset = m_auxilary_offset.x() * v + m_auxilary_offset.y() * n;
        \endcode
      - For those with offset_type() being \ref offset_square_cap,
        \ref offset_adjustable_cap_contour_start or
        \ref offset_adjustable_cap_contour_end the value the value
        is given by
        \code
        m_pre_offset + m_auxilary_offset
        \endcode
        In addition, \ref m_auxilary_offset the tangent vector along
        the path in the direction of the path and \ref m_pre_offset
        holds a vector perpindicular to the path or a zero vector
        (indicating it is not on boundary of cap).
     */
    vec2
    offset_vector(void);

    /*!
      When offset_type() is one of \ref offset_miter_clip_join,
      \ref offset_miter_clip_join_lambda_negated,
      \ref offset_miter_bevel_join or \ref offset_miter_join,
      returns the distance to the miter point. For other point
      types, returns 0.0.
     */
    float
    miter_distance(void) const;

    /*!
      Pack the data of this \ref point into a \ref
      PainterAttribute. The packing is as follows:
      - PainterAttribute::m_attrib0 .xy -> point::m_position (float)
      - PainterAttribute::m_attrib0 .zw -> point::m_pre_offset (float)
      - PainterAttribute::m_attrib1 .x  -> point::m_distance_from_edge_start (float)
      - PainterAttribute::m_attrib1 .y  -> point::m_distance_from_contour_start (float)
      - PainterAttribute::m_attrib1 .zw -> point::m_auxilary_offset (float)
      - PainterAttribute::m_attrib2 .x  -> point::m_packed_data (uint)
      - PainterAttribute::m_attrib2 .y  -> point::m_edge_length (float)
      - PainterAttribute::m_attrib2 .z  -> point::m_open_contour_length (float)
      - PainterAttribute::m_attrib2 .w  -> point::m_closed_contour_length (float)

      \param dst PainterAttribute to which to pack
     */
    void
    pack_point(PainterAttribute *dst) const;

    /*!
      Unpack a \ref point from a \ref PainterAttribute.
      \param dst point to which to unpack data
      \param src PainterAttribute from which to unpack data
     */
    static
    void
    unpack_point(point *dst, const PainterAttribute &src);
  };

  /*!
    \brief
    A Subset represents a handle to a portion of a StrokedPath
    for the purpose of drawing.
   */
  class Subset
  {
  public:
    /*!
      Ctor.
     */
    Subset(void);

    /*!
      Same usage as PainterAttributeData::attribute_data_chunks()
     */
    const_c_array<const_c_array<PainterAttribute> >
    attribute_data_chunks(void) const;

    /*!
      Same usage as PainterAttributeData::index_data_chunks()
     */
    const_c_array<const_c_array<PainterIndex> >
    index_data_chunks(void) const;

    /*!
      Same usage as PainterAttributeData::index_adjust_chunks()
     */
    const_c_array<int>
    index_adjust_chunks(void) const;

    /*!
      Same usage as PainterAttributeData::increment_z_values()

      Notes:
        - increment_z_value for Edge chunks is how many edges in the chunk
        - increment_z_value for Join chunks is how many joins in the chunk
        - increment_z_value for Cap chunks is how many caps in the chunk

      To make join culling effective:
        - on dashed stroking, each chunk is a join
        - on non-dashed stroking, a chunk can have multiple joins

      We will always make sure that chunking is:
        - edges, joins then caps
      to reduce vertex shader changes.

      When drawing, it is imperative to have the stuff in the PainterData
      realized as packed values (i.e. m_packed_value is valid and m_value
      is nullptr). Doing this prevents lots of reupload of PainterData
      values.
     */
    const_c_array<unsigned int>
    increment_z_values(void) const;
    
  private:
    friend class StrokedPath;
    reference_counter_ptr<opaque_reference_counted::base> m_d;
  };
  
  /*!
    \brief
    Opaque object to hold work room needed for functions
    of StrokedPath that require scratch space.
   */
  class ScratchSpace:fastuidraw::noncopyable
  {
  public:
    ScratchSpace(void);
    ~ScratchSpace();
  private:
    friend class StrokedPath;
    void *m_d;
  };

  /*!
    Ctor. Construct a StrokedPath from the data
    of a TessellatedPath.
    \param P source TessellatedPath
   */
  explicit
  StrokedPath(const TessellatedPath &P);

  ~StrokedPath();

  /*!
    Returns TessellatedPath::effective_curve_distance_threshhold()
    of the TessellatedPath that generated this StrokedPath.
   */
  float
  effective_curve_distance_threshhold(void) const;

  /*!
    Given a set of clip equations in clip coordinates
    and a tranformation from local coordiante to clip
    coordinates, compute what chunks are not completely
    culled by the clip equations.
    \param scratch_space scratch space for computations
    \param rounded_thresh rounded threshhold value to determine which caps and joins if
                          cap or join style is rounded
    \param js join style
    \param miter_limit indicates miter limit (to allow to cull miter joins), a negative
                       value indicates no miter limit
    \param cp cap style
    \param dash_evaluator if doing dashed stroking, the dash evalulator will cull
                          joins not to be drawn, if nullptr only those joins not in the
                          visible area defined by clip_equations are culled.
    \param clip_equations array of clip equations
    \param clip_matrix_local 3x3 transformation from local (x, y, 1)
                             coordinates to clip coordinates.
    \param recip_dimensions holds the reciprocal of the dimensions of the viewport
    \param pixels_additional_room amount in -pixels- to push clip equations by
                                  to grab additional edges
    \param item_space_additional_room amount in local coordinates to push clip
                                 equations by to grab additional edges
                                 draw the closing edges of each contour
    \param include_closing_edges if true include the chunks needed to
    \param max_attribute_cnt only allow those chunks for which have no more
                             than max_attribute_cnt attributes
    \param max_index_cnt only allow those chunks for which have no more
                         than max_index_cnt indices
    \param[out] out_subset DataWriter to which to write the results
   */
  void
  compute_subset(ScratchSpace &scratch_space,
                 float rounded_thresh,
                 enum PainterEnums::join_style js,
                 float miter_limit,
                 enum PainterEnums::cap_style cp,
                 DashEvaluatorBase *dash_evaluator,
                 const_c_array<vec3> clip_equations,
                 const float3x3 &clip_matrix_local,
                 const vec2 &recip_dimensions,
                 float pixels_additional_room,
                 float item_space_additional_room,
                 bool include_closing_edges,
                 unsigned int max_attribute_cnt,
                 unsigned int max_index_cnt,
                 Subset &out_subset) const;

private:
  void *m_d;
};

/*! @} */

}
