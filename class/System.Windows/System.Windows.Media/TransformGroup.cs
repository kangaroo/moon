//
// TransformGroup.cs
//
// Contact:
//   Moonlight List (moonlight-list@lists.ximian.com)
//
// Copyright 2007, 2009 Novell, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

using System.Windows.Markup;
using Mono;

namespace System.Windows.Media {

	public sealed partial class TransformGroup : Transform {

		public Matrix Value {
			get {
				IntPtr matrix = NativeMethods.general_transform_get_matrix (native);
				return new Matrix (matrix);
			}
		}
		
		public override GeneralTransform Inverse {
			get {
				Console.WriteLine ("System.Windows.Media.TransformGroup.Inverse : NIEX");
				throw new System.NotImplementedException ();
			}
		}
		
		public override bool TryTransform (Point inPoint, out Point outPoint)
		{
			Console.WriteLine ("System.Windows.Media.TransformGroup.TryTransform : NIEX");
			throw new System.NotImplementedException ();
		} 
	}
}
