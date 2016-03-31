#include <iostream>
#include <sstream>
#include <fstream>
#include <bitset>
#include <math.h>

#include <fastuidraw/util/util.hpp>

#include "sdl_painter_demo.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"
#include "read_path.hpp"
#include "ImageLoader.hpp"
#include "colorstop_command_line.hpp"
#include "cycle_value.hpp"

using namespace fastuidraw;

const char*
on_off(bool v)
{
  return v ? "ON" : "OFF";
}

class WindingValueFillRule:public Painter::CustomFillRuleBase
{
public:
  WindingValueFillRule(int v):
    m_winding_number(v)
  {}

  bool
  operator()(int w) const
  {
    return w == m_winding_number;
  }

private:
  int m_winding_number;
};

void
enable_wire_frame(bool b)
{
  #ifndef FASTUIDRAW_GL_USE_GLES
    {
      if(b)
        {
          glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
          glLineWidth(4.0);
        }
      else
        {
          glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
  #endif
}

class painter_stroke_test:public sdl_painter_demo
{
public:
  painter_stroke_test(void);


protected:

  void
  derived_init(int w, int h);

  void
  draw_frame(void);

  void
  handle_event(const SDL_Event &ev);

private:

  enum gradient_draw_mode
    {
      draw_no_gradient,
      draw_linear_gradient,
      draw_radial_gradient,

      number_gradient_draw_modes
    };

  enum image_filter
    {
      no_image,
      image_nearest_filter,
      image_linear_filter,
      image_cubic_filter,

      number_image_filter_modes
    };

  typedef std::pair<std::string, ColorStopSequenceOnAtlas::handle> named_color_stop;

  void
  construct_path(void);

  void
  create_stroked_path_attributes(void);

  void
  contruct_color_stops(void);

  vec2
  item_coordinates(ivec2 c);

  vec2
  brush_item_coordinate(ivec2 c);

  void
  update_cts_params(void);

  command_line_argument_value<int> m_max_segments_per_edge;
  command_line_argument_value<int> m_points_per_circle;
  command_line_argument_value<float> m_change_miter_limit_rate;
  command_line_argument_value<float> m_change_stroke_width_rate;
  command_line_argument_value<float> m_window_change_rate;
  command_line_argument_value<std::string> m_path_file;
  color_stop_arguments m_color_stop_args;
  command_line_argument_value<std::string> m_image_file;
  command_line_argument_value<unsigned int> m_image_slack;
  command_line_argument_value<int> m_sub_image_x, m_sub_image_y;
  command_line_argument_value<int> m_sub_image_w, m_sub_image_h;

  Path m_path;
  float m_max_miter;
  Image::handle m_image;
  uvec2 m_image_offset, m_image_size;
  std::vector<named_color_stop> m_color_stops;

  PanZoomTrackerSDLEvent m_zoomer;
  vecN<std::string, number_gradient_draw_modes> m_gradient_mode_labels;
  vecN<std::string, PainterEnums::number_cap_styles> m_cap_labels;
  vecN<std::string, PainterEnums::number_join_styles> m_join_labels;
  vecN<std::string, PainterEnums::fill_rule_data_count> m_fill_labels;
  vecN<std::string, number_image_filter_modes> m_image_filter_mode_labels;

  PainterState::PainterBrushState m_black_pen;
  PainterState::PainterBrushState m_white_pen;
  PainterState::PainterBrushState m_transparent_blue_pen;

  unsigned int m_join_style;
  unsigned int m_cap_style;
  unsigned int m_fill_rule;
  unsigned int m_end_fill_rule;
  bool m_have_miter_limit;
  float m_miter_limit, m_stroke_width;
  bool m_draw_fill;
  unsigned int m_active_color_stop;
  unsigned int m_gradient_draw_mode;
  bool m_repeat_gradient;
  unsigned int m_image_filter;

  vec2 m_gradient_p0, m_gradient_p1;
  float m_gradient_r0, m_gradient_r1;
  bool m_translate_brush, m_matrix_brush;

  bool m_repeat_window;
  vec2 m_repeat_xy, m_repeat_wh;

  bool m_clipping_window;
  vec2 m_clipping_xy, m_clipping_wh;

  bool m_stroke_aa;
  bool m_wire_frame;

  bool m_fill_by_clipping;

  simple_time m_draw_timer;
};

//////////////////////////////////////
// painter_stroke_test methods
painter_stroke_test::
painter_stroke_test(void):
  sdl_painter_demo("painter-stroke-test"),
  m_max_segments_per_edge(32, "max_segs", "Max number of segments per edge", *this),
  m_points_per_circle(60, "tess", "number of points per 2*PI curvature to attempt", *this),
  m_change_miter_limit_rate(1.0f, "miter_limit_rate",
                            "rate of change in in stroke widths per second for "
                            "changing the miter limit when the when key is down",
                            *this),
  m_change_stroke_width_rate(10.0f, "change_stroke_width_rate",
                             "rate of change in pixels/sec for changing stroke width "
                             "when changing stroke when key is down",
                             *this),
  m_window_change_rate(10.0f, "change_rate_brush_repeat_window",
                       "rate of change in pixels/sec when changing the repeat window",
                       *this),
  m_path_file("", "path_file",
              "if non-empty read the geometry of the path from the specified file, "
              "otherwise use a default path",
              *this),
  m_color_stop_args(*this),
  m_image_file("", "image", "if a valid file name, apply an image to drawing the fill", *this),
  m_image_slack(0, "image_slack", "amount of slack on tiles when loading image", *this),
  m_sub_image_x(0, "sub_image_x",
                "x-coordinate of top left corner of sub-image rectange (negative value means no-subimage)",
                *this),
  m_sub_image_y(0, "sub_image_y",
                "y-coordinate of top left corner of sub-image rectange (negative value means no-subimage)",
                *this),
  m_sub_image_w(-1, "sub_image_w",
                "sub-image width of sub-image rectange (negative value means no-subimage)",
                *this),
  m_sub_image_h(-1, "sub_image_h",
                "sub-image height of sub-image rectange (negative value means no-subimage)",
                *this),
  m_join_style(PainterEnums::rounded_joins),
  m_cap_style(PainterEnums::close_outlines),
  m_fill_rule(PainterEnums::odd_even_fill_rule),
  m_end_fill_rule(PainterEnums::fill_rule_data_count),
  m_have_miter_limit(true),
  m_miter_limit(5.0f),
  m_stroke_width(1.0f),
  m_draw_fill(false),
  m_active_color_stop(0),
  m_gradient_draw_mode(draw_no_gradient),
  m_repeat_gradient(true),
  m_image_filter(image_nearest_filter),
  m_translate_brush(false),
  m_matrix_brush(false),
  m_repeat_window(false),
  m_clipping_window(false),
  m_stroke_aa(true),
  m_wire_frame(false),
  m_fill_by_clipping(false)
{
  std::cout << "Controls:\n"
            << "\tj: cycle through join styles for stroking\n"
            << "\tc: cycle through cap style for stroking\n"
            << "\t[: decrease stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\t]: increase stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\tb: decrease miter limit(hold left-shift for slower rate and right shift for faster)\n"
            << "\tn: increase miter limit(hold left-shift for slower rate and right shift for faster)\n"
            << "\tm: toggle miter limit enforced\n"
            << "\tf: toggle drawing path fill\n"
            << "\tr: cycle through fill rules\n"
            << "\te: toggle fill by drawing clip rect\n"
            << "\ti: cycle through image filter to apply to fill (no image, nearest, linear, cubic)\n"
            << "\ts: cycle through defined color stops for gradient\n"
            << "\tg: cycle through gradient types (linear or radial)\n"
            << "\th: toggle repeat gradient\n"
            << "\tt: toggle translate brush\n"
            << "\ty: toggle matrix brush\n"
            << "\tp: toggle clipping window\n"
            << "\t4,6,2,8 (number pad): change location of clipping window\n"
            << "\tctrl-4,6,2,8 (number pad): change size of clipping window\n"
            << "\tw: toggle brush repeat window active\n"
            << "\tarrow keys: change location of brush repeat window\n"
            << "\tctrl-arrow keys: change size of brush repeat window\n"
            << "\tMiddle Mouse Draw: set p0(starting position top left) {drawn black with white inside} of gradient\n"
            << "\t1/2 : decrease/increase r0 of gradient(hold left-shift for slower rate and right shift for faster)\n"
            << "\t3/4 : decrease/increase r1 of gradient(hold left-shift for slower rate and right shift for faster)\n"
            << "\tRight Mouse Draw: set p1(starting position bottom right) {drawn white with black inside} of gradient\n"
            << "\tLeft Mouse Drag: pan\n"
            << "\tHold Left Mouse, then drag up/down: zoom out/in\n";

  m_gradient_mode_labels[draw_no_gradient] = "draw_no_gradient";
  m_gradient_mode_labels[draw_linear_gradient] = "draw_linear_gradient";
  m_gradient_mode_labels[draw_radial_gradient] = "draw_radial_gradient";

  m_join_labels[PainterEnums::no_joins] = "no_joins";
  m_join_labels[PainterEnums::rounded_joins] = "rounded_joins";
  m_join_labels[PainterEnums::bevel_joins] = "bevel_joins";
  m_join_labels[PainterEnums::miter_joins] = "miter_joins";

  m_cap_labels[PainterEnums::close_outlines] = "close_outlines";
  m_cap_labels[PainterEnums::no_caps] = "no_caps";
  m_cap_labels[PainterEnums::rounded_caps] = "rounded_caps";
  m_cap_labels[PainterEnums::square_caps] = "square_caps";

  m_fill_labels[PainterEnums::odd_even_fill_rule] = "odd_even_fill_rule";
  m_fill_labels[PainterEnums::nonzero_fill_rule] = "nonzero_fill_rule";
  m_fill_labels[PainterEnums::complement_odd_even_fill_rule] = "complement_odd_even_fill_rule";
  m_fill_labels[PainterEnums::complement_nonzero_fill_rule] = "complement_nonzero_fill_rule";

  m_image_filter_mode_labels[no_image] = "no_image";
  m_image_filter_mode_labels[image_nearest_filter] = "image_nearest_filter";
  m_image_filter_mode_labels[image_linear_filter] = "image_linear_filter";
  m_image_filter_mode_labels[image_cubic_filter] = "image_cubic_filter";
}


void
painter_stroke_test::
update_cts_params(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(NULL);
  assert(keyboard_state != NULL);

  float speed = static_cast<float>(m_draw_timer.restart());

  if(keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      speed *= 0.1f;
    }
  if(keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      speed *= 10.0f;
    }

  if(keyboard_state[SDL_SCANCODE_RIGHTBRACKET])
    {
      m_stroke_width += m_change_stroke_width_rate.m_value * speed / m_zoomer.transformation().scale();
    }

  if(keyboard_state[SDL_SCANCODE_LEFTBRACKET])
    {
      m_stroke_width -= m_change_stroke_width_rate.m_value  * speed / m_zoomer.transformation().scale();
      m_stroke_width = fastuidraw::t_max(m_stroke_width, 0.0f);
    }

  if(keyboard_state[SDL_SCANCODE_RIGHTBRACKET] || keyboard_state[SDL_SCANCODE_LEFTBRACKET])
    {
      std::cout << "Stroke width set to: " << m_stroke_width << "\n";
    }

  if(m_repeat_window)
    {
      vec2 *changer;
      float delta, delta_y;

      delta = m_window_change_rate.m_value * speed / m_zoomer.transformation().scale();
      if(keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          changer = &m_repeat_wh;
          delta_y = delta;
        }
      else
        {
          changer = &m_repeat_xy;
          delta_y = -delta;
        }

      if(keyboard_state[SDL_SCANCODE_UP])
        {
          changer->y() += delta_y;
          changer->y() = fastuidraw::t_max(0.0f, changer->y());
        }

      if(keyboard_state[SDL_SCANCODE_DOWN])
        {
          changer->y() -= delta_y;
          changer->y() = fastuidraw::t_max(0.0f, changer->y());
        }

      if(keyboard_state[SDL_SCANCODE_RIGHT])
        {
          changer->x() += delta;
        }

      if(keyboard_state[SDL_SCANCODE_LEFT])
        {
          changer->x() -= delta;
          changer->x() = fastuidraw::t_max(0.0f, changer->x());
        }

      if(keyboard_state[SDL_SCANCODE_UP] || keyboard_state[SDL_SCANCODE_DOWN]
         || keyboard_state[SDL_SCANCODE_RIGHT] || keyboard_state[SDL_SCANCODE_LEFT])
        {
          std::cout << "Brush repeat window set to: xy = " << m_repeat_xy << " wh = " << m_repeat_wh << "\n";
        }
    }


  if(m_gradient_draw_mode == draw_radial_gradient)
    {
      float delta;

      delta = m_window_change_rate.m_value * speed / m_zoomer.transformation().scale();
      if(keyboard_state[SDL_SCANCODE_1])
        {
          m_gradient_r0 -= delta;
          m_gradient_r0 = fastuidraw::t_max(0.0f, m_gradient_r0);
        }

      if(keyboard_state[SDL_SCANCODE_2])
        {
          m_gradient_r0 += delta;
        }
      if(keyboard_state[SDL_SCANCODE_3])
        {
          m_gradient_r1 -= delta;
          m_gradient_r1 = fastuidraw::t_max(0.0f, m_gradient_r1);
        }

      if(keyboard_state[SDL_SCANCODE_4])
        {
          m_gradient_r1 += delta;
        }

      if(keyboard_state[SDL_SCANCODE_1] || keyboard_state[SDL_SCANCODE_2]
         || keyboard_state[SDL_SCANCODE_3] || keyboard_state[SDL_SCANCODE_4])
        {
          std::cout << "Radial gradient values set to: r0 = "
                    << m_gradient_r0 << " r1 = " << m_gradient_r1 << "\n";
        }
    }


  if(m_join_style == PainterEnums::miter_joins && m_have_miter_limit)
    {
      if(keyboard_state[SDL_SCANCODE_N])
        {
          m_miter_limit += m_change_miter_limit_rate.m_value * speed;
          m_miter_limit = fastuidraw::t_min(m_max_miter, m_miter_limit);
        }

      if(keyboard_state[SDL_SCANCODE_B])
        {
          m_miter_limit -= m_change_miter_limit_rate.m_value * speed;
          m_miter_limit = fastuidraw::t_max(0.0f, m_miter_limit);
        }

      if(keyboard_state[SDL_SCANCODE_N] || keyboard_state[SDL_SCANCODE_B])
        {
          std::cout << "Miter-limit set to: " << m_miter_limit << "\n";
        }
    }

  if(m_clipping_window)
    {
      vec2 *changer;
      float delta, delta_y;

      delta = m_window_change_rate.m_value * speed / m_zoomer.transformation().scale();
      if(keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          changer = &m_clipping_wh;
          delta_y = delta;
        }
      else
        {
          changer = &m_clipping_xy;
          delta_y = -delta;
        }

      if(keyboard_state[SDL_SCANCODE_KP_8])
        {
          changer->y() += delta_y;
        }

      if(keyboard_state[SDL_SCANCODE_KP_2])
        {
          changer->y() -= delta_y;
        }

      if(keyboard_state[SDL_SCANCODE_KP_6])
        {
          changer->x() += delta;
        }

      if(keyboard_state[SDL_SCANCODE_KP_4])
        {
          changer->x() -= delta;
        }

      if(keyboard_state[SDL_SCANCODE_KP_2] || keyboard_state[SDL_SCANCODE_KP_4]
         || keyboard_state[SDL_SCANCODE_KP_6] || keyboard_state[SDL_SCANCODE_KP_8])
        {
          std::cout << "Clipping window set to: xy = " << m_clipping_xy << " wh = " << m_clipping_wh << "\n";
        }
    }
}



vec2
painter_stroke_test::
brush_item_coordinate(ivec2 c)
{
  vec2 p;
  p = item_coordinates(c);

  if(m_matrix_brush)
    {
      p *= m_zoomer.transformation().scale();
    }

  if(m_translate_brush)
    {
      p += m_zoomer.transformation().translation();
    }
  return p;
}

vec2
painter_stroke_test::
item_coordinates(ivec2 c)
{
  /* m_zoomer.transformation() gives the transformation
     from item coordiantes to screen coordinates. We
     Want the inverse.
   */
  vec2 p(c);
  return m_zoomer.transformation().apply_inverse_to_point(p);
}

void
painter_stroke_test::
handle_event(const SDL_Event &ev)
{
  m_zoomer.handle_event(ev);
  switch(ev.type)
    {
    case SDL_QUIT:
      end_demo(0);
      break;

    case SDL_WINDOWEVENT:
      if(ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          on_resize(ev.window.data1, ev.window.data2);
        }
      break;

    case SDL_MOUSEMOTION:
      {
        ivec2 c(ev.motion.x + ev.motion.xrel,
                ev.motion.y + ev.motion.yrel);

        if(ev.motion.state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
          {
            m_gradient_p0 = brush_item_coordinate(c);
          }
        else if(ev.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT))
          {
            m_gradient_p1 = brush_item_coordinate(c);
          }
      }
      break;

    case SDL_KEYUP:
      switch(ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo(0);
          break;

        case SDLK_a:
          m_stroke_aa = !m_stroke_aa;
          std::cout << "Anti-aliasing stroking = " << m_stroke_aa << "\n";
          break;

        case SDLK_p:
          m_clipping_window = !m_clipping_window;
          std::cout << "Clipping window: " << on_off(m_clipping_window) << "\n";
          break;

        case SDLK_w:
          m_repeat_window = !m_repeat_window;
          std::cout << "Brush Repeat window: " << on_off(m_repeat_window) << "\n";
          break;

        case SDLK_y:
          m_matrix_brush = !m_matrix_brush;
          std::cout << "Matrix brush: " << on_off(m_matrix_brush) << "\n";
          break;

        case SDLK_t:
          m_translate_brush = !m_translate_brush;
          std::cout << "Translate brush: " << on_off(m_translate_brush) << "\n";
          break;

        case SDLK_h:
          if(m_gradient_draw_mode != draw_no_gradient)
            {
              m_repeat_gradient = !m_repeat_gradient;
              if(!m_repeat_gradient)
                {
                  std::cout << "non-";
                }
              std::cout << "repeat gradient mode\n";
            }
          break;

        case SDLK_m:
          if(m_join_style == PainterEnums::miter_joins)
            {
              m_have_miter_limit = !m_have_miter_limit;
              std::cout << "Miter limit ";
              if(!m_have_miter_limit)
                {
                  std::cout << "NOT ";
                }
              std::cout << "applied\n";
            }
          break;

        case SDLK_i:
          if(m_image && m_draw_fill)
            {
              cycle_value(m_image_filter, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_image_filter_modes);
              std::cout << "Image filter mode set to: " << m_image_filter_mode_labels[m_image_filter] << "\n";
              if(m_image_filter == image_linear_filter && m_image->slack() < 1)
                {
                  std::cout << "\tWarning: image slack = " << m_image->slack()
                            << " which insufficient to correctly apply linear filter (requires atleast 1)\n";
                }
              else if(m_image_filter == image_cubic_filter && m_image->slack() < 2)
                {
                  std::cout << "\tWarning: image slack = " << m_image->slack()
                            << " which insufficient to correctly apply cubic filter (requires atleast 2)\n";
                }
            }
          break;

        case SDLK_s:
          if(m_draw_fill && !m_color_stops.empty())
            {
              cycle_value(m_active_color_stop, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_color_stops.size());
              std::cout << "Drawing color stop: " << m_color_stops[m_active_color_stop].first << "\n";
            }
          break;

        case SDLK_g:
          if(m_draw_fill && !m_color_stops.empty())
            {
              cycle_value(m_gradient_draw_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_gradient_draw_modes);
              std::cout << "Gradient mode set to: " << m_gradient_mode_labels[m_gradient_draw_mode] << "\n";
            }
          break;

        case SDLK_j:
          cycle_value(m_join_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), PainterEnums::number_join_styles);
          std::cout << "Join drawing mode set to: " << m_join_labels[m_join_style] << "\n";
          break;

        case SDLK_c:
          cycle_value(m_cap_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), PainterEnums::number_cap_styles);
          std::cout << "Cap drawing mode set to: " << m_cap_labels[m_cap_style] << "\n";
          break;

        case SDLK_r:
          if(m_draw_fill)
            {
              cycle_value(m_fill_rule, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_end_fill_rule);
              if(m_fill_rule < PainterEnums::fill_rule_data_count)
                {
                  std::cout << "Fill rule set to: " << m_fill_labels[m_fill_rule] << "\n";
                }
              else
                {
                  const_c_array<int> wnd;
                  int value;
                  wnd = m_path.tessellation()->filled()->winding_numbers();
                  value = wnd[m_fill_rule - PainterEnums::fill_rule_data_count];
                  std::cout << "Fill rule set to custom fill rule: winding_number == "
                            << value << "\n";
                }
            }
          break;

        case SDLK_e:
          if(m_draw_fill)
            {
              m_fill_by_clipping = !m_fill_by_clipping;
              std::cout << "Set to ";
              if(m_fill_by_clipping)
                {
                  std::cout << "fill by drawing rectangle clipped to path\n";
                }
              else
                {
                  std::cout << "fill by drawing fill\n";
                }
            }
          break;

        case SDLK_f:
          m_draw_fill = !m_draw_fill;
          std::cout << "Set to ";
          if(!m_draw_fill)
            {
              std::cout << "NOT ";
            }
          std::cout << "draw path fill\n";
          break;

        case SDLK_SPACE:
          m_wire_frame = !m_wire_frame;
          std::cout << "Wire Frame = " << m_wire_frame << "\n";
          break;
        }
      break;
    };
}

void
painter_stroke_test::
construct_path(void)
{
  if(!m_path_file.m_value.empty())
    {
      std::ifstream path_file(m_path_file.m_value.c_str());
      if(path_file)
        {
          std::stringstream buffer;
          buffer << path_file.rdbuf();
          read_path(m_path, buffer.str());
          return;
        }
    }


  m_path << vec2(300.0f, 300.0f)
         << Path::end()
         << vec2(50.0f, 35.0f)
         << Path::control_point(60.0f, 50.0f)
         << vec2(70.0f, 35.0f)
         << Path::arc_degrees(180.0, vec2(70.0f, -100.0f))
         << Path::control_point(60.0f, -150.0f)
         << Path::control_point(30.0f, -50.0f)
         << vec2(0.0f, -100.0f)
         << Path::end_arc_degrees(90.0f)
         << vec2(200.0f, 200.0f)
         << vec2(400.0f, 200.0f)
         << vec2(400.0f, 400.0f)
         << vec2(200.0f, 400.0f)
         << Path::end();
}

void
painter_stroke_test::
create_stroked_path_attributes(void)
{
  TessellatedPath::TessellationParams P;
  P.m_max_segments = m_max_segments_per_edge.m_value;
  P.m_curve_tessellation = 2.0f * float(M_PI) / static_cast<float>(m_points_per_circle.m_value);
  m_path.tessellation_params(P);

  m_max_miter = 0.0f;
  const_c_array<StrokedPath::point> miter_points;
  miter_points = m_path.tessellation()->stroked()->miter_joins_points(true);
  for(unsigned p = 0, endp = miter_points.size(); p < endp; ++p)
    {
      float v;

      v = miter_points[p].m_miter_distance;
      if(isfinite(v))
        {
          m_max_miter = fastuidraw::t_max(m_max_miter, fastuidraw::t_abs(v) );
        }
    }
  m_miter_limit = fastuidraw::t_min(100.0f, m_max_miter); //100 is an insane miter limit.
}

void
painter_stroke_test::
contruct_color_stops(void)
{
  for(color_stop_arguments::hoard::const_iterator
        iter = m_color_stop_args.m_values.begin(),
        end = m_color_stop_args.m_values.end();
      iter != end; ++iter)
    {
      ColorStopSequenceOnAtlas::handle h;
      h = FASTUIDRAWnew ColorStopSequenceOnAtlas(iter->second->m_stops,
                                                m_painter->colorstop_atlas(),
                                                iter->second->m_discretization);
      m_color_stops.push_back(named_color_stop(iter->first, h));
    }
}

void
painter_stroke_test::
draw_frame(void)
{
  update_cts_params();
  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  enable_wire_frame(m_wire_frame);

  m_painter->begin();

  ivec2 wh(dimensions());
  float3x3 proj(float_orthogonal_projection_params(0, wh.x(), wh.y(), 0)), m;
  m = proj * m_zoomer.transformation().matrix3();
  m_painter->transformation(m);

  if(m_clipping_window)
    {
      m_painter->clipInRect(m_clipping_xy, m_clipping_wh);
    }

  if(m_translate_brush)
    {
      m_painter->brush().transformation_translate(m_zoomer.transformation().translation());
    }
  else
    {
      m_painter->brush().no_transformation_translation();
    }

  if(m_matrix_brush)
    {
      float2x2 m;
      m(0, 0) = m(1, 1) = m_zoomer.transformation().scale();
      m_painter->brush().transformation_matrix(m);
    }
  else
    {
      m_painter->brush().no_transformation_matrix();
    }

  if(m_repeat_window)
    {
      m_painter->brush().repeat_window(m_repeat_xy, m_repeat_wh);
    }
  else
    {
      m_painter->brush().no_repeat_window();
    }

  if(m_draw_fill)
    {
      m_painter->brush().pen(1.0f, 1.0f, 1.0f, 1.0f);

      if(m_gradient_draw_mode == draw_linear_gradient && !m_color_stops.empty())
        {
          m_painter->brush().linear_gradient(m_color_stops[m_active_color_stop].second,
                                             m_gradient_p0, m_gradient_p1, m_repeat_gradient);
        }
      else if(m_gradient_draw_mode == draw_radial_gradient && !m_color_stops.empty())
        {
          m_painter->brush().radial_gradient(m_color_stops[m_active_color_stop].second,
                                             m_gradient_p0, m_gradient_r0,
                                             m_gradient_p1, m_gradient_r1,
                                             m_repeat_gradient);
        }
      else
        {
          m_painter->brush().no_gradient();
        }


      if(!m_image || m_image_filter == no_image)
        {
          m_painter->brush().no_image();
        }
      else
        {
          enum PainterBrush::image_filter f;
          switch(m_image_filter)
            {
            case image_nearest_filter:
              f = PainterBrush::image_filter_nearest;
              break;
            case image_linear_filter:
              f = PainterBrush::image_filter_linear;
              break;
            case image_cubic_filter:
              f = PainterBrush::image_filter_cubic;
              break;
            default:
              assert(!"Incorrect value for m_image_filter!");
              f = PainterBrush::image_filter_nearest;
            }
          m_painter->brush().sub_image(m_image, m_image_offset, m_image_size, f);
        }

      if(m_fill_rule < PainterEnums::fill_rule_data_count)
        {
          enum PainterEnums::fill_rule_t v;
          v = static_cast<enum PainterEnums::fill_rule_t>(m_fill_rule);
          if(m_fill_by_clipping)
            {
              m_painter->save();
              enable_wire_frame(false);
              m_painter->clipInPath(m_path, v);
              enable_wire_frame(m_wire_frame);
              m_painter->transformation(float3x3());
              m_painter->draw_rect(vec2(-1.0f, -1.0f), vec2(2.0f, 2.0f));
              m_painter->restore();
            }
          else
            {
              m_painter->fill_path(m_path, v);
            }
        }
      else
        {
          const_c_array<int> wnd;
          int value;

          wnd = m_path.tessellation()->filled()->winding_numbers();
          value = wnd[m_fill_rule - PainterEnums::fill_rule_data_count];

          if(m_fill_by_clipping)
            {
              m_painter->save();
              enable_wire_frame(false);
              m_painter->clipInPath(m_path, WindingValueFillRule(value));
              enable_wire_frame(m_wire_frame);
              m_painter->transformation(float3x3());
              m_painter->draw_rect(vec2(-1.0f, -1.0f), vec2(2.0f, 2.0f));
              m_painter->restore();
            }
          else
            {
              m_painter->fill_path(m_path, WindingValueFillRule(value));
            }
        }
      m_painter->brush().no_image();
      m_painter->brush().no_gradient();
    }

  if(!m_transparent_blue_pen)
    {
      m_painter->brush().pen(0.0f, 0.0f, 1.0f, 0.5f);
      m_transparent_blue_pen = m_painter->brush_state();
    }

  m_painter->brush_state(m_transparent_blue_pen);


  if(m_stroke_width > 0.0f)
    {
      PainterState::StrokeParams st;
      if(m_have_miter_limit)
        {
          st.miter_limit(m_miter_limit);
        }
      else
        {
          st.miter_limit(-1.0f);
        }

      st.width(m_stroke_width);
      m_painter->vertex_shader_data(st);
      m_painter->stroke_path(m_path,
                             static_cast<enum PainterEnums::cap_style>(m_cap_style),
                             static_cast<enum PainterEnums::join_style>(m_join_style),
                             m_stroke_aa);
    }

  if(m_draw_fill && m_gradient_draw_mode != draw_no_gradient)
    {
      float r0, r1;
      vec2 p0, p1;
      float inv_scale;

      inv_scale = 1.0f / m_zoomer.transformation().scale();
      r0 = 15.0f * inv_scale;
      r1 = 30.0f * inv_scale;

      p0 = m_gradient_p0;
      p1 = m_gradient_p1;


      if(m_translate_brush)
        {
          p0 -= m_zoomer.transformation().translation();
          p1 -= m_zoomer.transformation().translation();
        }

      if(m_matrix_brush)
        {
          p0 *= inv_scale;
          p1 *= inv_scale;
        }

      if(!m_black_pen)
        {
          assert(!m_white_pen);
          m_painter->brush().reset();

          m_painter->brush().pen(0.0, 0.0, 0.0, 1.0);
          m_black_pen = m_painter->brush_state();

          m_painter->brush().pen(1.0, 1.0, 1.0, 1.0);
          m_white_pen = m_painter->brush_state();
        }

      m_painter->brush_state(m_black_pen);
      m_painter->draw_rect(p0 - vec2(r1) * 0.5, vec2(r1));
      m_painter->brush_state(m_white_pen);
      m_painter->draw_rect(p0 - vec2(r0) * 0.5, vec2(r0));

      m_painter->brush_state(m_white_pen);
      m_painter->draw_rect(p1 - vec2(r1) * 0.5, vec2(r1));
      m_painter->brush_state(m_black_pen);
      m_painter->draw_rect(p1 - vec2(r0) * 0.5, vec2(r0));
    }
  m_painter->end();
}

void
painter_stroke_test::
derived_init(int w, int h)
{
  //put into unit of per ms.
  m_window_change_rate.m_value /= 1000.0f;
  m_change_stroke_width_rate.m_value /= 1000.0f;
  m_change_miter_limit_rate.m_value /= 1000.0f;

  construct_path();
  create_stroked_path_attributes();
  contruct_color_stops();
  m_end_fill_rule = m_path.tessellation()->filled()->winding_numbers().size() + PainterEnums::fill_rule_data_count;

  /* set transformation to center and contain path.
   */
  vec2 p0, p1, delta, dsp(w, h), ratio, mid;
  float mm;
  p0 = m_path.tessellation()->bounding_box_min();
  p1 = m_path.tessellation()->bounding_box_max();

  delta = p1 - p0;
  ratio = delta / dsp;
  mm = fastuidraw::t_max(0.00001f, fastuidraw::t_max(ratio.x(), ratio.y()) );
  mid = 0.5 * (p1 + p0);

  ScaleTranslate<float> sc, tr1, tr2;
  tr1.translation(-mid);
  sc.scale( 1.0f / mm);
  tr2.translation(dsp * 0.5f);
  m_zoomer.transformation(tr2 * sc * tr1);

  if(!m_image_file.m_value.empty())
    {
      std::vector<u8vec4> image_data;
      ivec2 image_size;

      image_size = load_image_to_array(m_image_file.m_value, image_data);
      if(image_size.x() > 0 && image_size.y() > 0)
        {
          m_image = Image::create(m_painter->image_atlas(), image_size.x(), image_size.y(),
                                  cast_c_array(image_data), m_image_slack.m_value);
        }
    }

  if(m_image)
    {
      if(m_sub_image_x.m_value < 0 || m_sub_image_y.m_value < 0
         || m_sub_image_w.m_value < 0 || m_sub_image_h.m_value < 0)
        {
          m_image_offset = uvec2(0, 0);
          m_image_size = uvec2(m_image->dimensions());
        }
      else
        {
          m_image_offset = uvec2(m_sub_image_x.m_value, m_sub_image_y.m_value);
          m_image_size = uvec2(m_sub_image_w.m_value, m_sub_image_h.m_value);
        }
    }

  m_gradient_p0 = item_coordinates(ivec2(0, 0));
  m_gradient_p1 = item_coordinates(ivec2(w, h));

  m_gradient_r0 = 0.0f;
  m_gradient_r1 = 200.0f / m_zoomer.transformation().scale();

  m_repeat_xy = vec2(0.0f, 0.0f);
  m_repeat_wh = m_path.tessellation()->bounding_box_max() - m_path.tessellation()->bounding_box_min();

  m_clipping_xy = m_path.tessellation()->bounding_box_min();
  m_clipping_wh = m_repeat_wh;

  m_draw_timer.restart();
}

int
main(int argc, char **argv)
{
  painter_stroke_test P;
  return P.main(argc, argv);
}
