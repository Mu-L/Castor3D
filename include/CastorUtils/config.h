/*
This source file is part of Castor3D (http://dragonjoker.co.cc

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
the program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
*/
#ifndef ___Castor_Config___
#define ___Castor_Config___

//! Tells whether or not to use doubles for type real (if not, floats are used)
#undef CASTOR_USE_DOUBLE
#define CASTOR_USE_DOUBLE 0

//! Tells whether or not to use Castor containers
#undef CASTOR_USE_MT_CONTAINERS
#define CASTOR_USE_MT_CONTAINERS 0

//! Tells whether or not to use MultiThreading in Castor dependant projects, check MultithreadConfig.h to configure multithreading
#undef CASTOR_USE_MULTITHREADING
#define CASTOR_USE_MULTITHREADING 1

#endif