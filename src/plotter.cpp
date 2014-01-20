/* This file is part of the Pangolin Project.
 * http://github.com/stevenlovegrove/Pangolin
 *
 * Copyright (c) 2014 Steven Lovegrove
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <pangolin/plotter.h>
#include <pangolin/gldraw.h>

#include <iomanip>

namespace pangolin
{

inline void SetColor(float colour[4], float r, float g, float b, float alpha = 1.0f)
{
    colour[0] = r;
    colour[1] = g;
    colour[2] = b;
    colour[3] = alpha;
}

std::string ReplaceChar(const std::string& str, char from, char to)
{
    std::string ret = str;
    for(size_t i=0; i<ret.length(); ++i) {
        if(ret[i] == from) ret[i] = to;
    }
    return ret;
}

std::set<int> ConvertSequences(const std::string& str, char seq_char='$', char id_char='i')
{
    std::set<int> sequences;

    for(size_t i=0; i<str.length(); ++i) {
        if(str[i] == seq_char) {
            if(i+1 < str.length() && str[i+1] == id_char) {
                sequences.insert(-1);
            }else{
                int v = 0;
                for(size_t j=i+1; std::isdigit(str[j]) && j< str.length(); ++j) {
                    v = v*10 + (str[j] - '0');
                }
                sequences.insert(v);
            }
        }
    }

    return sequences;
}

Plotter::PlotSeries::PlotSeries()
    : drawing_mode(GL_LINE_STRIP)
{

}

void Plotter::PlotSeries::CreatePlot(const std::string &x, const std::string &y, Colour colour, std::string title)
{
    static const std::string vs_header =
            "uniform int u_id_offset;\n"
            "uniform vec4 u_color;\n"
            "uniform vec2 u_scale;\n"
            "uniform vec2 u_offset;\n"
            "varying vec4 v_color;\n"
            "void main() {\n";

    static const std::string vs_footer =
            "    vec2 pos = vec2(x, y);\n"
            "    gl_Position = vec4(u_scale * (pos + u_offset),0,1);\n"
            "    v_color = u_color;\n"
            "}\n";

    static const std::string fs =
            "varying vec4 v_color;\n"
            "void main() {\n"
            "  gl_FragColor = v_color;\n"
            "}\n";

    attribs.clear();

    this->colour = colour;
    this->title  = GlFont::I().Text(title.c_str());
    const std::set<int> ax = ConvertSequences(x);
    const std::set<int> ay = ConvertSequences(y);
    std::set<int> as;
    as.insert(ax.begin(), ax.end());
    as.insert(ay.begin(), ay.end());
    contains_id = ( as.find(-1) != as.end() );

    std::string vs_attrib;
    for(std::set<int>::const_iterator i=as.begin(); i != as.end(); ++i) {
        std::ostringstream oss;
        oss << "s" << *i;
        const std::string name = *i >= 0 ? oss.str() : "si";
        attribs.push_back( PlotAttrib(name, *i) );
        vs_attrib += "attribute float " + name + ";\n";
    }

    prog.AddShader( GlSlVertexShader,
                    vs_attrib +
                    vs_header +
                    "float x = " + ReplaceChar(x,'$','s') + ";\n" +
                    "float y = " + ReplaceChar(y,'$','s') + ";\n" +
                    vs_footer);
    prog.AddShader( GlSlFragmentShader, fs );
    prog.Link();

    // Lookup attribute locations in compiled shader
    prog.SaveBind();
    for(size_t i=0; i<attribs.size(); ++i) {
        attribs[i].location = prog.GetAttributeHandle( attribs[i].name );
    }
    prog.Unbind();
}

void Plotter::PlotImplicit::CreatePlot(const std::string& code)
{
    static const std::string vs =
            "attribute vec2 a_position;\n"
            "uniform vec2 u_scale;\n"
            "uniform vec2 u_offset;\n"
            "varying float x;\n"
            "varying float y;\n"
            "void main() {\n"
            "    gl_Position = vec4(u_scale * (a_position + u_offset),0,1);\n"
            "    x = a_position.x;"
            "    y = a_position.y;"
            "}\n";

    static const std::string fs1 =
            "varying float x;\n"
            "varying float y;\n"
            "void main() {\n";
    static const std::string fs2 =
            "}\n";

    prog.AddShader( GlSlVertexShader, vs );
    prog.AddShader( GlSlFragmentShader, fs1 + code + fs2 );
    prog.BindPangolinDefaultAttribLocations();
    prog.Link();

}


void Plotter::PlotImplicit::CreateColouredPlot(const std::string& code)
{
    CreatePlot(
        "  float r=1.0;\n"
        "  float g=1.0;\n"
        "  float b=1.0;\n"
        "  float a=0.5;\n" +
           code +
        "  gl_FragColor = vec4(r,g,b,a);\n"
        );
}

void Plotter::PlotImplicit::CreateInequality(const std::string& ie, Colour c)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "if( !(" << ie << ") ) discard;\n";
    oss << "gl_FragColor = vec4(" << c.r << "," << c.g << "," << c.b << "," << c.a << ");\n";

    CreatePlot( oss.str() );
}

void Plotter::PlotImplicit::CreateDistancePlot(const std::string& dist)
{

}

Plotter::Plotter(DataLog* log, float left, float right, float bottom, float top, float tickx, float ticky, Plotter* linked)
    : colour_wheel(0.6)
{
    if(!log) {
        throw std::runtime_error("DataLog not specified");
    }

    // Handle our own mouse / keyboard events
    this->handler = this;
    this->log = log;

    // Default colour scheme
    colour_bg = Colour(0.0,0.0,0.0);
    colour_tk = Colour(0.2,0.2,0.2);
    colour_ms = Colour(0.3,0.3,0.3);
    colour_ax = Colour(0.5,0.5,0.5);

    // Setup view range.
    SetView(left,right,bottom,top);
    ticks[0] = tickx;
    ticks[1] = ticky;
    track_front = false;
    lineThickness = 1.5;

    // Create shader for drawing simple primitives
    prog_default.AddShader( GlSlVertexShader,
                         "attribute vec2 a_position;\n"
                         "uniform vec4 u_color;\n"
                         "uniform vec2 u_scale;\n"
                         "uniform vec2 u_offset;\n"
                         "varying vec4 v_color;\n"
                         "void main() {\n"
                         "    gl_Position = vec4(u_scale * (a_position + u_offset),0,1);\n"
                         "    v_color = u_color;\n"
                         "}\n"
                         );
    prog_default.AddShader( GlSlFragmentShader,
                         "varying vec4 v_color;\n"
                         "void main() {\n"
                         "  gl_FragColor = v_color;\n"
                         "}\n"
                         );
    prog_default.BindPangolinDefaultAttribLocations();
    prog_default.Link();

    prog_default_tex.AddShader( GlSlVertexShader,
                         "attribute vec2 a_position;\n"
                         "attribute vec2 a_texcoord;\n"
                         "uniform vec4 u_color;\n"
                         "uniform vec2 u_scale;\n"
                         "uniform vec2 u_offset;\n"
                         "varying vec4 v_color;\n"
                         "varying vec2 v_texcoord;\n"
                         "void main() {\n"
                         "    gl_Position = vec4(u_scale * (a_position + u_offset),0,1);\n"
                         "    v_color = u_color;\n"
                         "    v_texcoord = a_texcoord;\n"
                         "}\n"
                         );
    prog_default_tex.AddShader( GlSlFragmentShader,
                         "varying vec4 v_color;\n"
                         "varying vec2 v_texcoord;\n"
                         "uniform sampler2D u_texture;\n"
                         "void main() {\n"
                         "  gl_FragColor = v_color;\n"
                         "  gl_FragColor.a *= texture2D(u_texture, v_texcoord).a;\n"
                         "}\n"
                         );
    prog_default_tex.BindPangolinDefaultAttribLocations();
    prog_default_tex.Link();


    // Setup default PlotSeries
    plotseries.reserve(10);
    for(int i=0; i<10; ++i) {
        std::ostringstream oss;
        oss << "$" << i;
        plotseries.push_back( PlotSeries() );
        plotseries.back().CreatePlot("$i", oss.str(), colour_wheel.GetUniqueColour(), oss.str() );
    }

//    // Setup test PlotMarkers
//    plotmarkers.push_back( PlotMarker(true, -1, 0.1, 1.0, 0.0, 0.0, 0.2 ) );
//    plotmarkers.push_back( PlotMarker(false, 1, 2*M_PI/0.01, 0.0, 1.0, 0.0, 0.2 ) );

    // Setup test implicit plots.
    plotimplicits.reserve(10);
//    plotimplicits.push_back( PlotImplicit() );
//    plotimplicits.back().CreateInequality("x+y <= 150.0", colour_wheel.GetUniqueColour().WithAlpha(0.2) );
//    plotimplicits.push_back( PlotImplicit() );
//    plotimplicits.back().CreateInequality("x+2.0*y <= 170.0", colour_wheel.GetUniqueColour().WithAlpha(0.2) );
//    plotimplicits.push_back( PlotImplicit() );
//    plotimplicits.back().CreateInequality("3.0*y <= 180.0", colour_wheel.GetUniqueColour().WithAlpha(0.2) );

    // Setup texture spectogram style plots
    // ...

}

void Plotter::Render()
{
    UpdateView();

#ifndef HAVE_GLES
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT);
#endif

    if( track_front )
    {
        // TODO: Implement
    }

    glClearColor(colour_bg.r, colour_bg.g, colour_bg.b, colour_bg.a);
    ActivateScissorAndClear();

    // Try to create smooth lines
    glDisable(GL_MULTISAMPLE);
    glLineWidth(1.5);
    glEnable(GL_LINE_SMOOTH);
    glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    glDisable( GL_DEPTH_TEST );

    const float x = int_x[0];
    const float y = int_y[0];
    const float w = int_x[1] - x;
    const float h = int_y[1] - y;
    const float ox = -(x+w/2.0f);
    const float oy = -(y+h/2.0f);

    //////////////////////////////////////////////////////////////////////////
    // Draw ticks
    prog_default.SaveBind();
    prog_default.SetUniform("u_scale",  2.0f / w, 2.0f / h);
    prog_default.SetUniform("u_offset", ox, oy);
    prog_default.SetUniform("u_color",  colour_tk );
    glLineWidth(lineThickness);
    const int tx[2] = {
        (int)ceil(int_x[0] / ticks[0]),
        (int)ceil(int_x[1] / ticks[0])
    };

    const int ty[2] = {
        (int)ceil(int_y[0] / ticks[1]),
        (int)ceil(int_y[1] / ticks[1])
    };

    if( tx[1] - tx[0] < v.w/4 ) {
        for( int i=tx[0]; i<tx[1]; ++i ) {
            glDrawLine((i)*ticks[0], int_y[0],   (i)*ticks[0], int_y[1]);
        }
    }

    if( ty[1] - ty[0] < v.h/4 ) {
        for( int i=ty[0]; i<ty[1]; ++i ) {
            glDrawLine(int_x[0], (i)*ticks[1],  int_x[1], (i)*ticks[1]);
        }
    }
    prog_default.Unbind();

    //////////////////////////////////////////////////////////////////////////
    // Draw axis

    prog_default.SaveBind();
    prog_default.SetUniform("u_color",  colour_ax );
    glDrawLine(0, int_y[0],  0, int_y[1] );
    glDrawLine(int_x[0],0,   int_x[1],0  );
    prog_default.Unbind();

    //////////////////////////////////////////////////////////////////////////
    // Draw Implicits

    for(size_t i=0; i < plotimplicits.size(); ++i) {
        PlotImplicit& im = plotimplicits[i];
        im.prog.SaveBind();

        im.prog.SetUniform("u_scale",  2.0f / w, 2.0f / h);
        im.prog.SetUniform("u_offset", ox, oy);

        glDrawRect(int_x[0], int_y[0], int_x[1], int_y[1]);

        im.prog.Unbind();
    }

    //////////////////////////////////////////////////////////////////////////
    // Draw series

    static size_t id_start = 0;
    static size_t id_size = 0;
    static float* id_array = 0;

    for(size_t i=0; i < plotseries.size(); ++i)
    {
        PlotSeries& ps = plotseries[i];
        GlSlProgram& prog = ps.prog;
        ps.used = false;

        prog.SaveBind();
        prog.SetUniform("u_scale",  2.0f / w, 2.0f / h);
        prog.SetUniform("u_offset", ox, oy);
        prog.SetUniform("u_color", ps.colour );

        const DataLogBlock* block = log->Blocks();
        while(block) {
            if(ps.contains_id ) {
                if(id_size < block->Samples() ) {
                    // Create index array that we can bind
                    delete[] id_array;
                    id_start = -1;
                    id_size = block->MaxSamples();
                    id_array = new float[id_size];
                }
                if(id_start != block->StartId()) {
                    for(size_t k=0; k < id_size; ++k) {
                        id_array[k] = block->StartId() + k;
                    }
                }
            }

            prog.SetUniform("u_id_offset",  (int)block->StartId() );

            // Enable appropriate attributes
            bool shouldRender = true;
            for(size_t i=0; i< ps.attribs.size(); ++i) {
                if(0 <= ps.attribs[i].plot_id && ps.attribs[i].plot_id < (int)block->Dimensions() ) {
                    glVertexAttribPointer(ps.attribs[i].location, 1, GL_FLOAT, GL_FALSE, block->Dimensions()*sizeof(float), block->DimData(ps.attribs[i].plot_id) );
                    glEnableVertexAttribArray(ps.attribs[i].location);
                }else if( ps.attribs[i].plot_id == -1 ){
                    glVertexAttribPointer(ps.attribs[i].location, 1, GL_FLOAT, GL_FALSE, 0, id_array );
                    glEnableVertexAttribArray(ps.attribs[i].location);
                }else{
                    // bad id: don't render
                    shouldRender = false;
                    break;
                }
            }

            if(shouldRender) {
                // Draw geometry
                glDrawArrays(ps.drawing_mode, 0, block->Samples());
                ps.used = true;
            }

            // Disable enabled attributes
            for(size_t i=0; i< ps.attribs.size(); ++i) {
                glDisableVertexAttribArray(ps.attribs[i].location);
            }

            block = block->NextBlock();
        }
        prog.Unbind();
    }

    prog_default.SaveBind();

    //////////////////////////////////////////////////////////////////////////
    // Draw markers
    glLineWidth(2.5f);

    for( size_t i=0; i < plotmarkers.size(); ++i) {
        PlotMarker& m = plotmarkers[i];
        prog_default.SetUniform("u_color",  m.colour );
        if(m.horizontal) {
            if(m.leg == 0) {
                glDrawLine(int_x[0], m.coord,  int_x[1], m.coord );
            }else if(m.leg == -1) {
                glDrawRect(int_x[0], int_y[0],  int_x[1], m.coord);
            }else if(m.leg == 1) {
                glDrawRect(int_x[0], m.coord,  int_x[1], int_y[1]);
            }
        }else{
            if(m.leg == 0) {
                glDrawLine(m.coord, int_y[0],  m.coord, int_y[1] );
            }else if(m.leg == -1) {
                glDrawRect(int_x[0], int_y[0],  m.coord, int_y[1] );
            }else if(m.leg == 1) {
                glDrawRect(m.coord, int_y[0],  int_x[1], int_y[1] );
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Draw hover / selection
    glLineWidth(1.5f);

    // hover over
    prog_default.SetUniform("u_color",  colour_ax.WithAlpha(0.3) );
    glDrawLine(hover[0], int_y[0],  hover[0], int_y[1] );
    glDrawLine(int_x[0], hover[1],  int_x[1], hover[1] );

    // range
    prog_default.SetUniform("u_color",  colour_ax.WithAlpha(0.5) );
    glDrawLine(sel_x[0], int_y[0],  sel_x[0], int_y[1] );
    glDrawLine(sel_x[1], int_y[0],  sel_x[1], int_y[1] );
    glDrawLine(int_x[0], sel_y[0],  int_x[1], sel_y[0] );
    glDrawLine(int_x[0], sel_y[1],  int_x[1], sel_y[1] );
    glDrawRect(sel_x[0], sel_y[0],  sel_x[1], sel_y[1]);

    prog_default.Unbind();

    prog_default_tex.SaveBind();

    //////////////////////////////////////////////////////////////////////////
    // Draw Key

    prog_default_tex.SetUniform("u_scale",  2.0f / v.w, 2.0f / v.h);

    int keyid = 0;
    for(size_t i=0; i < plotseries.size(); ++i)
    {
        PlotSeries& ps = plotseries[i];
        if(ps.used) {
            prog_default_tex.SetUniform("u_color", ps.colour );
            prog_default_tex.SetUniform("u_offset",v.w-25 -(v.w/2.0f), v.h-15*(++keyid) -(v.h/2.0f));
            ps.title.DrawGlSl();
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Draw axis text

    prog_default_tex.SetUniform("u_scale",  2.0f / v.w, 2.0f / v.h);
    prog_default_tex.SetUniform("u_color", colour_ax );

    if( tx[1] - tx[0] < v.w/4 ) {
        for( int i=tx[0]; i<tx[1]; ++i ) {
            std::ostringstream oss;
            const float divpi = ticks[0] / M_PI;
            const float divrt2 = ticks[0] / M_SQRT2;
            if( std::abs(divpi - floor(divpi)) < 1E-6 ) {
                oss << i*divpi << "pi";
            }else if( std::abs(divrt2 - floor(divrt2)) < 1E-6 ) {
                oss << i*divpi << "sqrt(2)";
            }else{
                oss << i*ticks[0];
            }
            GlText txt = GlFont::I().Text(oss.str().c_str());
            float sx = v.w*((i)*ticks[0]-(int_x[0]+int_x[1])/2.0f)/w - txt.Width()/2.0f;
            prog_default_tex.SetUniform("u_offset", sx, 15 -v.h/2.0f );
            txt.DrawGlSl();
        }
    }

    if( ty[1] - ty[0] < v.h/4 ) {
        for( int i=ty[0]; i<ty[1]; ++i ) {
            std::ostringstream oss;
            const float divpi = ticks[1] / M_PI;
            const float divrt2 = ticks[1] / M_SQRT2;
            if( std::abs(divpi - floor(divpi)) < 1E-6 ) {
                oss << i*divpi << "pi";
            }else if( std::abs(divrt2 - floor(divrt2)) < 1E-6 ) {
                oss << i*divpi << "sqrt(2)";
            }else{
                oss << i*ticks[1];
            }
            GlText txt = GlFont::I().Text(oss.str().c_str());
            float sy = v.h*((i)*ticks[1]-(int_y[0]+int_y[1])/2.0f)/h - txt.Height()/2.0f;
            prog_default_tex.SetUniform("u_offset", 15 -v.w/2.0f, sy );
            txt.DrawGlSl();
        }
    }


    prog_default_tex.Unbind();


    glLineWidth(1.0f);

#ifndef HAVE_GLES
    glPopAttrib();
#endif

}

void Plotter::SetViewPan(float left, float right, float bottom, float top)
{
    target_x[0] = left;
    target_x[1] = right;
    target_y[0] = bottom;
    target_y[1] = top;
}

void Plotter::SetView(float left, float right, float bottom, float top)
{
    SetViewPan(left,right,bottom,top);
    int_x[0] = left;
    int_x[1] = right;
    int_y[0] = bottom;
    int_y[1] = top;
}

void Plotter::FixSelection()
{
    // Make sure selection matches sign of current viewport
    if( (sel_x[0]<sel_x[1]) != (int_x[0]<int_x[1]) ) {
        std::swap(sel_x[0], sel_x[1]);
    }
    if( (sel_y[0]<sel_y[1]) != (int_y[0]<int_y[1]) ) {
        std::swap(sel_y[0], sel_y[1]);
    }
}

void Plotter::UpdateView()
{
    const float sf = 1.0 / 20.0;
    float dx[2] = { target_x[0]-int_x[0], target_x[1]-int_x[1] };
    float dy[2] = { target_y[0]-int_y[0], target_y[1]-int_y[1] };
    int_x[0] += sf * dx[0];
    int_x[1] += sf * dx[1];
    int_y[0] += sf * dy[0];
    int_y[1] += sf * dy[1];
}

void Plotter::ScrollView(float x, float y)
{
    int_x[0] += x; int_x[1] += x;
    int_y[0] += y; int_y[1] += y;
    target_x[0] += x; target_x[1] += x;
    target_y[0] += y; target_y[1] += y;
}

void Plotter::ScaleView(float x, float y)
{
    const double c[2] = {
        track_front ? int_x[1] : (int_x[0] + int_x[1])/2.0,
        (int_y[0] + int_y[1])/2.0
    };

    int_x[0] = x*(int_x[0] - c[0]) + c[0];
    int_x[1] = x*(int_x[1] - c[0]) + c[0];
    int_y[0] = y*(int_y[0] - c[1]) + c[1];
    int_y[1] = y*(int_y[1] - c[1]) + c[1];

    target_x[0] = x*(target_x[0] - c[0]) + c[0];
    target_x[1] = x*(target_x[1] - c[0]) + c[0];
    target_y[0] = y*(target_y[0] - c[1]) + c[1];
    target_y[1] = y*(target_y[1] - c[1]) + c[1];
}

void Plotter::Keyboard(View&, unsigned char key, int x, int y, bool pressed)
{
    const float mvfactor = 1.0 / 10.0;

    if(pressed) {
        if(key == ' ' && sel_x[0] != sel_x[1] && sel_y[0] != sel_y[1]) {
            // Set view to equal selection
            SetViewPan(sel_x[0], sel_x[1], sel_y[0], sel_y[1]);

            // Reset selection
            sel_x[1] = sel_x[0];
            sel_y[1] = sel_y[0];
        }else if(key == PANGO_SPECIAL + PANGO_KEY_LEFT) {
            const float w = int_x[1] - int_x[0];
            const float dx = mvfactor*w;
            target_x[0] -= dx;
            target_x[1] -= dx;
        }else if(key == PANGO_SPECIAL + PANGO_KEY_RIGHT) {
            const float w = target_x[1] - target_x[0];
            const float dx = mvfactor*w;
            target_x[0] += dx;
            target_x[1] += dx;
        }else if(key == PANGO_SPECIAL + PANGO_KEY_UP) {
            const float h = target_y[1] - target_y[0];
            const float dy = mvfactor*h;
            target_y[0] += dy;
            target_y[1] += dy;
        }else if(key == PANGO_SPECIAL + PANGO_KEY_DOWN) {
            const float h = target_y[1] - target_y[0];
            const float dy = mvfactor*h;
            target_y[0] -= dy;
            target_y[1] -= dy;
        }
    }
}

void Plotter::ScreenToPlot(int xpix, int ypix, float& xplot, float& yplot)
{
    xplot = int_x[0] + (int_x[1]-int_x[0]) * (xpix - v.l) / (float)v.w;
    yplot = int_y[0] + (int_y[1]-int_y[0]) * (ypix - v.b) / (float)v.h;
}

void Plotter::Mouse(View& view, MouseButton button, int x, int y, bool pressed, int button_state)
{
    last_mouse_pos[0] = x;
    last_mouse_pos[1] = y;

    if(button == MouseButtonLeft) {
        // Update selected range
        if(pressed) {
            ScreenToPlot(x,y, sel_x[0], sel_y[0]);
        }
        ScreenToPlot(x,y, sel_x[1], sel_y[1]);
    }else if(button == MouseWheelUp || button == MouseWheelDown) {
        Special(view, InputSpecialZoom, x, y, ((button == MouseWheelDown) ? 0.1 : -0.1), 0.0f, 0.0f, 0.0f, button_state );
    }

    FixSelection();
}

void Plotter::MouseMotion(View& view, int x, int y, int button_state)
{
    const int d[2] = {x-last_mouse_pos[0],y-last_mouse_pos[1]};
    const float is[2] = {int_x[1]-int_x[0],int_y[1]-int_y[0]};
    const float df[2] = {is[0]*d[0]/(float)v.w, is[1]*d[1]/(float)v.h};

    if( button_state == MouseButtonLeft )
    {
        // Update selected range
        ScreenToPlot(x,y, sel_x[1], sel_y[1]);
    }else if(button_state == MouseButtonMiddle )
    {
        Special(view, InputSpecialScroll, df[0], df[1], 0.0f, 0.0f, 0.0f, 0.0f, button_state);
    }else if(button_state == MouseButtonRight )
    {
        const float scale[2] = {
            1.0f + (float)d[0] / (float)v.w,
            1.0f - (float)d[1] / (float)v.h,
        };
        ScaleView(scale[0], scale[1]);
    }

    // Update hover status (after potential resizing)
    ScreenToPlot(x, y, hover[0], hover[1]);

    last_mouse_pos[0] = x;
    last_mouse_pos[1] = y;
}

void Plotter::PassiveMouseMotion(View&, int x, int y, int button_state)
{
    ScreenToPlot(x, y, hover[0], hover[1]);
}

void Plotter::Special(View&, InputSpecial inType, float x, float y, float p1, float p2, float p3, float p4, int button_state)
{
    if(inType == InputSpecialScroll) {
        const float d[2] = {p1,-p2};
        const float is[2] = {int_x[1]-int_x[0],int_y[1]-int_y[0]};
        const float df[2] = {is[0]*d[0]/(float)v.w, is[1]*d[1]/(float)v.h};

        ScrollView(-df[0], -df[1]);

        if(df[0] > 0) {
            track_front = false;
        }
    } else if(inType == InputSpecialZoom) {
        float scalex = 1.0;
        float scaley = 1.0;

        if(button_state & KeyModifierCmd) {
            scaley = 1-p1;
        }else{
            scalex = 1-p1;
        }

        ScaleView(scalex, scaley);

//        const double c[2] = {
//            track_front ? int_x[1] : (int_x[0] + int_x[1])/2.0,
//            (int_y[0] + int_y[1])/2.0
//        };

//        if(button_state & KeyModifierCmd) {
//            int_y[0] = scale*(int_y[0] - c[1]) + c[1];
//            int_y[1] = scale*(int_y[1] - c[1]) + c[1];
//        }else{
//            int_x[0] = scale*(int_x[0] - c[0]) + c[0];
//            int_x[1] = scale*(int_x[1] - c[0]) + c[0];
//        }
    }

    // Update hover status (after potential resizing)
    ScreenToPlot(x, y, hover[0], hover[1]);
}

}
