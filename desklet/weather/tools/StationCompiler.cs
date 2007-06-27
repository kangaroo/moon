/*
 * StationCompiler.cs: Moonlight Desklets Weather Desklet station compiler.
 *
 * Author:
 *   Marek Habersack (mhabersack@novell.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 *
 *
 * This compiler uses the Location.xml file from the gnome weather applet as its input.
 * Currently, the translations are ignored (they should be used to generate .po files
 * later on)
 */

using System;
using System.IO;
using System.CodeDom;
using System.CodeDom.Compiler;
using System.Collections.Generic;
using System.Reflection;
using System.Xml;

using Microsoft.CSharp;

class Element
{
	public string Name;
}

class Location : Element
{
	public string Code;
	public string Coordinates;
}

class City : Element
{
	public List <Location> Locations;

	public City ()
	{
		Locations = new List <Location> ();
	}
}

class State : Element
{
	public List <Element> Locations;

	public State ()
	{
		Locations = new List <Element> ();
	}
}

class Country : Element
{
	public List <Element> Locations;

	public Country ()
	{
		Locations = new List <Element> ();
	}
}

class Region : Element
{
	public List <Country> Countries;

	public Region ()
	{
		Countries = new List <Country> ();
	}
}

public class App
{
	static Stack<Element> elements;
	static List<Region> regions;
	
	static void Usage ()
	{
		Console.WriteLine ("Usage:\n\tStationCompiler Path/To/Locations.xml OUTPUT_FILE.cs");
		Environment.Exit (1);
	}

	static void Die (string format, params object[] parms)
	{
		Console.WriteLine (format, parms);
		Environment.Exit (1);
	}

	static void StartRegion ()
	{
		Region r = new Region ();
		regions.Add (r);
		elements.Push (r);
	}

	static void EndRegion ()
	{
		Region r = elements.Peek () as Region;

		if (r == null)
			throw new ApplicationException ("</region> parsed but the current element is not a Region");
		elements.Pop ();
	}
	
	static void StartCountry ()
	{
		Region r = elements.Peek () as Region;

		if (r == null)
			throw new ApplicationException ("The parent element is not a <region>");
		
		Country c = new Country ();
		elements.Push (c);
		r.Countries.Add (c);
	}

	static void EndCountry ()
	{
		Country c = elements.Peek () as Country;

		if (c == null)
			throw new ApplicationException ("</country> parsed but the current element is not a Country");
		elements.Pop ();
	}
	
	static void StartLocation ()
	{
		Element c = elements.Peek ();

		if (!(c is Country) && !(c is City) && !(c is State))
			throw new ApplicationException ("The parent element is not a <country>, <state> or <city>");
		
		Location l = new Location ();
		elements.Push (l);
		if (c is Country)
			((Country)c).Locations.Add (l);
		else if (c is City)
			((City)c).Locations.Add (l);
		else
			((State)c).Locations.Add (l);
	}
	
	static void EndLocation ()
	{
		Location l = elements.Peek () as Location;

		if (l == null)
			throw new ApplicationException ("</location> parsed but the current element is not a Location");
		elements.Pop ();
	}

	static void StartCity ()
	{
		Element c = elements.Peek ();

		if (!(c is Country) && !(c is State))
			throw new ApplicationException ("The parent element is not a <country> or <state>");
		
		City l = new City ();
		elements.Push (l);
		if (c is Country)
			((Country)c).Locations.Add (l);
		else
			((State)c).Locations.Add (l);
	}

	static void EndCity ()
	{
		City l = elements.Peek () as City;

		if (l == null)
			throw new ApplicationException ("</city> parsed but the current element is not a City");
		elements.Pop ();
	}

	static void StartState ()
	{
		Country c = elements.Peek () as Country;

		if (c == null)
			throw new ApplicationException ("The parent element is not a <country>");
		
		State s = new State ();
		elements.Push (s);
		c.Locations.Add (s);
	}

	static void EndState ()
	{
		State s = elements.Peek () as State;

		if (s == null)
			throw new ApplicationException ("</state> parsed but the current element is not a State");
		elements.Pop ();
	}
	
	static void SetName (XmlNode node)
	{
		Element e = elements.Peek ();

		if (e == null)
			throw new ApplicationException ("<name> parsed but no element to apply it to");


		if (node.Attributes ["xml:lang"] != null) {
			// store for .po
			return;
		}
		
		XmlNodeList children = node.ChildNodes;
		if (children == null || children.Count == 0)
			return;

		XmlNode child;
		for (int i = 0; i < children.Count; i++) {
			child = children [i];
			if (child.NodeType != XmlNodeType.Text)
				continue;
			
			e.Name += child.Value.Trim ();
		}
	}

	static void SetCode (XmlNode node)
	{
		Location l = elements.Peek () as Location;

		if (l == null)
			throw new ApplicationException ("<code> parsed but the parent element is not a Location");

		XmlNodeList children = node.ChildNodes;
		if (children == null || children.Count == 0)
			return;

		XmlNode child;
		for (int i = 0; i < children.Count; i++) {
			child = children [i];
			if (child.NodeType != XmlNodeType.Text)
				continue;
			
			l.Code += child.Value.Trim ();
		}
	}

	static void SetCoordinates (XmlNode node)
	{
		Location l = elements.Peek () as Location;

		if (l == null)
			throw new ApplicationException ("<coordinates> parsed but the parent element is not a Location");

		XmlNodeList children = node.ChildNodes;
		if (children == null || children.Count == 0)
			return;

		XmlNode child;
		for (int i = 0; i < children.Count; i++) {
			child = children [i];
			if (child.NodeType != XmlNodeType.Text)
				continue;
			
			l.Coordinates += child.Value.Trim ();
		}
	}

	static void ProcessLocations (XmlNode node)
	{
		switch (node.NodeType) {
			case XmlNodeType.Comment:
			default:
				return;
				
			case XmlNodeType.Element:
				switch (node.LocalName) {
					case "region":
						StartRegion ();
						break;

					case "country":
						StartCountry ();
						break;

					case "state":
						StartState ();
						break;
						
					case "location":
						StartLocation ();
						break;

					case "city":
						StartCity ();
						break;
						
					case "name":
						SetName (node);
						break;

					case "code":
						SetCode (node);
						break;
						
					case "coordinates":
						SetCoordinates (node);
						break;
				}
				break;				

			case XmlNodeType.Text:
				break;
		}

		XmlNodeList children = node.ChildNodes;
		if (children != null || children.Count > 0)
			for (int i = 0; i < children.Count; i++)
				ProcessLocations (children [i]);
		
		switch (node.LocalName) {
			case "region":
				EndRegion ();
				break;

			case "country":
				EndCountry ();
				break;

			case "state":
				EndState ();
				break;
				
			case "location":
				EndLocation ();
				break;

			case "city":
				EndCity ();
				break;
		}
	}
	
	static void BuildLocations (string inputFile, string outputFile)
	{
		XmlDocument doc = new XmlDocument ();
		doc.Load (inputFile);
		XmlElement root = doc.DocumentElement;

		elements = new Stack <Element> ();
		regions = new List <Region> ();
		
		ProcessLocations (root);
		CompileLocations (outputFile);
	}

	static CodeObjectCreateExpression BuildCityLocation (Location l, CodeMemberMethod method)
	{
		return new CodeObjectCreateExpression (locationType,
						       new CodeExpression[] {
							       new CodePrimitiveExpression (l.Name),
							       new CodePrimitiveExpression (l.Code),
							       new CodePrimitiveExpression (l.Coordinates)
						       });
	}
	
	static void BuildCity (City c, CodeMemberMethod method, CodeVariableReferenceExpression curVar,
			       CodeVariableReferenceExpression curLocation)
	{
		CodeAssignStatement cityAssign = new CodeAssignStatement ();
		cityAssign.Left = curCity;
		cityAssign.Right = new CodeCastExpression (cityType, curLocation);
		method.Statements.Add (cityAssign);

		CodeMethodInvokeExpression locationAdd;
		
		foreach (Location l in c.Locations) {
			locationAdd = new CodeMethodInvokeExpression (
				curCity,
				"Add",
				new CodeExpression[] {BuildCityLocation (l, method)}
			);
			method.Statements.Add (locationAdd);
		}
	}

	static CodeObjectCreateExpression BuildStateLocation (Element e, CodeMemberMethod method)
	{
		CodeObjectCreateExpression location;

		if (e is Location)
			location = new CodeObjectCreateExpression (locationType,
								   new CodeExpression[] {
									   new CodePrimitiveExpression (e.Name),
									   new CodePrimitiveExpression (((Location)e).Code),
									   new CodePrimitiveExpression (((Location)e).Coordinates)
								   });
		else if (e is City)
			location = new CodeObjectCreateExpression (cityType,
								   new CodeExpression[] {
									   new CodePrimitiveExpression (e.Name),
									   new CodePrimitiveExpression (((City)e).Locations.Count)
								   });
		else
			throw new ApplicationException (String.Format ("Unexpected <state> child type: {0}", e));

		if (e is City) {
			CodeAssignStatement locationAssign = new CodeAssignStatement ();
			locationAssign.Left = curStateLocation;
			locationAssign.Right = location;
			method.Statements.Add (locationAssign);
			
			BuildCity (e as City, method, curState, curStateLocation);
			return null;
		}

		return location;
	}
	
	static void BuildState (State s, CodeMemberMethod method)
	{
		CodeAssignStatement stateAssign = new CodeAssignStatement ();
		stateAssign.Left = curState;
		stateAssign.Right = new CodeCastExpression (stateType, curCountryLocation);
		method.Statements.Add (stateAssign);

		CodeMethodInvokeExpression locationAdd = new CodeMethodInvokeExpression (
			curState,
			"Add",
			new CodeExpression[] {curStateLocation}
		);
		CodeMethodInvokeExpression locationAddDirect;
		
		CodeObjectCreateExpression expr;
		foreach (Element e in s.Locations) {
			expr = BuildStateLocation (e, method);
			if (expr == null)
				method.Statements.Add (locationAdd);
			else {
				locationAddDirect = locationAdd = new CodeMethodInvokeExpression (
					curState,
					"Add",
					new CodeExpression[] {expr});
				method.Statements.Add (locationAddDirect);
			}
		}
	}
	
	static CodeObjectCreateExpression BuildCountryLocation (Element e, CodeMemberMethod method)
	{
		CodeObjectCreateExpression location;

		if (e is Location)
			location = new CodeObjectCreateExpression (locationType,
								   new CodeExpression[] {
									   new CodePrimitiveExpression (e.Name),
									   new CodePrimitiveExpression (((Location)e).Code),
									   new CodePrimitiveExpression (((Location)e).Coordinates)
								   });
		else if (e is City)
			location = new CodeObjectCreateExpression (cityType,
								   new CodeExpression[] {
									   new CodePrimitiveExpression (e.Name),
									   new CodePrimitiveExpression (((City)e).Locations.Count)
								   });
		else if (e is State)
			location = new CodeObjectCreateExpression (stateType,
								   new CodeExpression[] {
									   new CodePrimitiveExpression (e.Name),
									   new CodePrimitiveExpression (((State)e).Locations.Count)
								   });
		else
			throw new ApplicationException (String.Format ("Unexpected <country> child type: {0}", e));

		if (e is Location)
			return location;
		
		CodeAssignStatement locationAssign = new CodeAssignStatement ();
		locationAssign.Left = curCountryLocation;
		locationAssign.Right = location;
		method.Statements.Add (locationAssign);

		if (e is City)
			BuildCity (e as City, method, curCountry, curCountryLocation);
		else if (e is State)
			BuildState (e as State, method);

		return null;
	}
	
	static void BuildCountry (Country c, CodeMemberMethod method)
	{
		CodeObjectCreateExpression country =
			new CodeObjectCreateExpression (countryType,
							new CodeExpression[] {
								new CodePrimitiveExpression (c.Name),
								new CodePrimitiveExpression (c.Locations.Count)
							});
		CodeAssignStatement countryAssign = new CodeAssignStatement ();
		countryAssign.Left = curCountry;
		countryAssign.Right = country;
		method.Statements.Add (countryAssign);

		CodeMethodInvokeExpression locationAdd = new CodeMethodInvokeExpression (
			curCountry,
			"Add",
			new CodeExpression[] {curCountryLocation}
		);
		CodeMethodInvokeExpression locationAddDirect;
		
		CodeObjectCreateExpression expr;
		
		foreach (Element e in c.Locations) {
			expr = BuildCountryLocation (e, method);
			if (expr == null)
				method.Statements.Add (locationAdd);
			else {
				locationAddDirect = new CodeMethodInvokeExpression (
					curCountry,
					"Add",
					new CodeExpression[] {expr}
				);
				method.Statements.Add (locationAddDirect);
			}
		}
	}
	
	static void BuildRegion (Region r, CodeMemberMethod method)
	{
		CodeObjectCreateExpression region =
			new CodeObjectCreateExpression (regionType,
							new CodeExpression[] {
								new CodePrimitiveExpression (r.Name),
								new CodePrimitiveExpression (r.Countries.Count)
							});
		CodeAssignStatement regionAssign = new CodeAssignStatement ();
		regionAssign.Left = curRegion;
		regionAssign.Right = region;
		method.Statements.Add (regionAssign);

		CodeMethodInvokeExpression regionAdd = new CodeMethodInvokeExpression (
			curRegion,
			"Add",
			new CodeExpression[] {curCountry}
		);
		
		foreach (Country c in r.Countries) {
			BuildCountry (c, method);
			method.Statements.Add (regionAdd);
		}
	}

	static CodeThisReferenceExpression thisref = new CodeThisReferenceExpression ();
	static CodeTypeReference regionType = new CodeTypeReference ("Region");
	static CodeTypeReference countryType = new CodeTypeReference ("Country");
	static CodeTypeReference stateType = new CodeTypeReference ("State");
	static CodeTypeReference locationType = new CodeTypeReference ("Location");
	static CodeTypeReference cityType = new CodeTypeReference ("City");
	
	static CodeVariableReferenceExpression curRegion = new CodeVariableReferenceExpression ("curRegion");
	static CodeVariableReferenceExpression curCountry = new CodeVariableReferenceExpression ("curCountry");
	static CodeVariableReferenceExpression curCountryLocation = new CodeVariableReferenceExpression ("curCountryLocation");
	static CodeVariableReferenceExpression curCity = new CodeVariableReferenceExpression ("curCity");
	static CodeVariableReferenceExpression curState = new CodeVariableReferenceExpression ("curState");
	static CodeVariableReferenceExpression curStateLocation = new CodeVariableReferenceExpression ("curStateLocation");
	
	static void GenerateBuildData (CodeMemberMethod method)
	{
		CodeTypeReference regionListType = new CodeTypeReference ("List",
									  new CodeTypeReference[] {
										  new CodeTypeReference ("Region")
									  });
		CodeObjectCreateExpression newObject =
			new CodeObjectCreateExpression (regionListType,
							new CodeExpression[] {new CodePrimitiveExpression (regions.Count)});
		CodeAssignStatement regionsAssign = new CodeAssignStatement ();
		regionsAssign.Left = new CodeFieldReferenceExpression (thisref, "regions");
		regionsAssign.Right = newObject;
		method.Statements.Add (regionsAssign);

		CodeVariableDeclarationStatement var = new CodeVariableDeclarationStatement ("Region", "curRegion");
		method.Statements.Add (var);
		var = new CodeVariableDeclarationStatement ("Country", "curCountry");
		method.Statements.Add (var);
		var = new CodeVariableDeclarationStatement ("Element", "curCountryLocation");
		method.Statements.Add (var);
		var = new CodeVariableDeclarationStatement ("City", "curCity");
		method.Statements.Add (var);
		var = new CodeVariableDeclarationStatement ("State", "curState");
		method.Statements.Add (var);
		var = new CodeVariableDeclarationStatement ("Element", "curStateLocation");
		method.Statements.Add (var);
		
		foreach (Region r in regions)
			BuildRegion (r, method);
	}
	
	static void CompileLocations (string outputFile)
	{
		CodeCompileUnit unit = new CodeCompileUnit ();
		CodeNamespace ns = new CodeNamespace ("Desklet.Weather");

		unit.Namespaces.Add (ns);
		ns.Imports.Add (new CodeNamespaceImport ("System"));
		ns.Imports.Add (new CodeNamespaceImport ("System.Collections.Generic"));

		CodeTypeDeclaration mainClass = new CodeTypeDeclaration ("Locations");
		mainClass.TypeAttributes = TypeAttributes.Class | TypeAttributes.Sealed | TypeAttributes.Public;
		mainClass.IsPartial = true;
		ns.Types.Add (mainClass);

		CodeMemberMethod buildData = new CodeMemberMethod ();
		buildData.Name = "BuildData";
		GenerateBuildData (buildData);
		mainClass.Members.Add (buildData);
		
		CodeDomProvider provider = new CSharpCodeProvider ();
		ICodeGenerator gen = provider.CreateGenerator ();

		TextWriter tw = new IndentedTextWriter (new StreamWriter (outputFile, false), "\t");
		gen.GenerateCodeFromCompileUnit (unit, tw, new CodeGeneratorOptions ());
		tw.Close ();
	}
	
	public static void Main (string[] args)
	{
		if (args.Length < 2)
			Usage ();

		if (!File.Exists (args [0]))
			Die ("Location file {0} does not exist.", args [0]);

		BuildLocations (args [0], args [1]);
	}
}
