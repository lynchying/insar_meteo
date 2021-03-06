#!/usr/bin/env python3

# Copyright (C) 2018  István Bozsó
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
Plot scattered points.
"""

import os
import os.path as pth
import argparse as ap

from inmet.gmt import GMT, get_ranges, raster_parser, gen_tuple, multi_parser

def parse_arguments():
    parser = ap.ArgumentParser(description=__doc__,
            formatter_class=ap.ArgumentDefaultsHelpFormatter,
            parents=[raster_parser, multi_parser])

    parser.add_argument(
        "infile",
        type=str,
        help="Binary inputfile.")

    parser.add_argument(
        "ncols",
        type=int,
        help="Number of data columns in inputfile.")
    
    parser.add_argument(
        "--out",
        nargs="?",
        default=None,
        type=str,
        help="Postscript or raster outputfile.")
    
    parser.add_argument(
        "-p", "--proj",
        nargs="?",
        default="J",
        type=str,
        help="Projection of the maps.")
    
    parser.add_argument(
        "-i", "--idx",
        nargs="?",
        default=None,
        type=gen_tuple(int),
        help="Column indices to work with. List of integers.")
        
    parser.add_argument(
        "--titles",
        nargs="?",
        default=None,
        type=gen_tuple(str),
        help="Titles of the plots. List of strings.")
    
    parser.add_argument(
        "--tryaxis",
        action="store_true")
    
    parser.add_argument(
        "--xy_range",
        nargs="?",
        default=None,
        type=gen_tuple(float),
        help="Range of x and y axis. List of floats.")
    
    parser.add_argument(
        "--z_range",
        nargs="?",
        default=None,
        type=gen_tuple(float),
        help="Range of z values. List of floats.")

    parser.add_argument(
        "--cpt",
        nargs="?",
        default="drywet",
        type=str,
        help="GMT colorscale.")

    parser.add_argument(
        "--x_axis",
        nargs="?",
        default="a0.5g0.25f0.25",
        type=str,
        help="GMT configuration of x axis.")

    parser.add_argument(
        "--y_axis",
        nargs="?",
        default="a0.25g0.25f0.25",
        type=str,
        help="GMT configuration of y axis.")
    
    parser.add_argument(
        "--xy_add",
        nargs="?",
        default=0.05,
        type=float,
        help="Extension of x and y range.")

    parser.add_argument(
        "--z_add",
        nargs="?",
        default=0.1,
        type=float,
        help="Extension of z range.")
    
    parser.add_argument(
        "--mode",
        nargs="?",
        default="v",
        type=str,
        help="Colorbar mode, set it to v for vertical or h for horizontal.")
        
    parser.add_argument(
        "--label",
        nargs="?",
        default=None,
        type=str,
        help="Colorbar label. Input of the -B flag of psscale.")
        
    parser.add_argument(
        "--offset",
        nargs="?",
        default=25.0,
        type=float,
        help="Colorbar offset towards the margins.")

    parser.add_argument(
        "--step",
        nargs="?",
        default=None,
        type=float,
        help="Step of colorbar ticks.")
    
    return parser.parse_args()
    
def main():
    args = parse_arguments()
    
    infile = args.infile
    
    # default is "${infile}.png"
    if args.out is None:
        out = pth.basename(args.infile).split(".")[0] + ".png"
    else:
        out = args.out
    
    name, ext = pth.splitext(out)
    
    if ext != ".ps":
        ps_file = name + ".ps"
    else:
        ps_file = out
    
    # 2 additional coloumns for coordinates, float64s are expected
    bindef = "{}d".format(args.ncols + 2)
    
    if args.xy_range is None or args.z_range is None:
        _xy_range, _z_range = get_ranges(data=infile, binary=bindef,
                                         xy_add=args.xy_add, z_add=args.z_add)
    
    if args.xy_range is None:
        xy_range = _xy_range
    else:
        xy_range = args.xy_range

    if args.z_range is None:
        z_range = _z_range
    else:
        z_range = args.z_range
    

    if args.idx is None:
        idx = range(args.ncols)
    else:
        idx = args.idx
    
    # parse titles
    if args.titles is None:
        titles = range(1, args.ncols + 1)
    else:
        titles = args.titles
    
    gmt = GMT(ps_file, R=xy_range)
    x, y = gmt.multiplot(len(idx), args.proj, nrows=args.nrows,
                         right=args.right, top=args.top, left=args.left,
                         xpad=args.hpad, ypad=args.vpad)
    
    gmt.makecpt("tmp.cpt", C=args.cpt, Z=True, T=z_range)
    
    x_axis = args.x_axis
    y_axis = args.y_axis
    
    # do not plot the scatter points yet just test the placement of basemaps
    if args.tryaxis:
        for ii in idx:
            input_format = "0,1,{}".format(ii + 2)
            gmt.psbasemap(Xf="{}p".format(x[ii]), Yf="{}p".format(y[ii]),
                          B="WSen+t{}".format(titles[ii]), Bx=x_axis, By=y_axis)
    else:
        for ii in idx:
            input_format = "0,1,{}".format(ii + 2)
            gmt.psbasemap(Xf="{}p".format(x[ii]), Yf="{}p".format(y[ii]),
                          B="WSen+t{}".format(titles[ii]), Bx=x_axis, By=y_axis)

            gmt.psxy(data=infile, i=input_format, bi=bindef, S="c0.025c",
                     C="tmp.cpt")

    if args.step is None:
        label = "5"
    else:
        label = str(args.step)
    
    if args.label is not None:
        label += args.label
    
    gmt.colorbar(mode=args.mode, offset=args.offset, B=label, C="tmp.cpt")
    
    if ext != ".ps":
        gmt.raster(out, dpi=args.dpi, gray=args.gray, portrait=args.portrait,
                   with_pagesize=args.pagesize, multi_page=args.multi_page,
                   transparent=args.transparent)
        os.remove(ps_file)
    
    os.remove("tmp.cpt")
    
    del gmt
