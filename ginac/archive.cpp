/** @file archive.cpp
 *
 *  Archiving of GiNaC expressions. */

/*
 *  GiNaC Copyright (C) 1999-2000 Johannes Gutenberg University Mainz, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <iostream>
#include <stdexcept>

#include "archive.h"
#include "registrar.h"
#include "ex.h"
#include "config.h"
#include "utils.h"

#ifndef NO_NAMESPACE_GINAC
namespace GiNaC {
#endif // ndef NO_NAMESPACE_GINAC


/** Archive an expression.
 *  @param ex  the expression to be archived
 *  @param name  name under which the expression is stored */
void archive::archive_ex(const ex &e, const char *name)
{
	// Create root node (which recursively archives the whole expression tree)
	// and add it to the archive
	archive_node_id id = add_node(archive_node(*this, e));

	// Add root node ID to list of archived expressions
	archived_ex ae = archived_ex(atomize(name), id);
	exprs.push_back(ae);
}


/** Add archive_node to archive if the corresponding expression is
 *  not already archived.
 *  @return ID of archived node */
archive_node_id archive::add_node(const archive_node &n)
{
	// Search for node in nodes vector
	vector<archive_node>::const_iterator i = nodes.begin(), iend = nodes.end();
	archive_node_id id = 0;
	while (i != iend) {
		if (i->has_same_ex_as(n))
			return id;
		i++; id++;
	}

	// Not found, add archive_node to nodes vector
	nodes.push_back(n);
	return id;
}


/** Retrieve archive_node by ID. */
archive_node &archive::get_node(archive_node_id id)
{
	if (id >= nodes.size())
		throw (std::range_error("archive::get_node(): archive node ID out of range"));

	return nodes[id];
}


/** Retrieve expression from archive by name.
 *  @param sym_lst  list of pre-defined symbols */
ex archive::unarchive_ex(const lst &sym_lst, const char *name) const
{
	// Find root node
	string name_string = name;
	archive_atom id = atomize(name_string);
	vector<archived_ex>::const_iterator i = exprs.begin(), iend = exprs.end();
	while (i != iend) {
		if (i->name == id)
			goto found;
		i++;
	}
	throw (std::logic_error("expression with name '" + name_string + "' not found in archive"));

found:
	// Recursively unarchive all nodes, starting at the root node
	return nodes[i->root].unarchive(sym_lst);
}

/** Retrieve expression from archive by index.
 *  @param sym_lst  list of pre-defined symbols */
ex archive::unarchive_ex(const lst &sym_lst, unsigned int index) const
{
	if (index >= exprs.size())
		throw (std::range_error("index of archived expression out of range"));

	// Recursively unarchive all nodes, starting at the root node
	return nodes[exprs[index].root].unarchive(sym_lst);
}

/** Retrieve expression and its name from archive by index.
 *  @param sym_lst  list of pre-defined symbols */
ex archive::unarchive_ex(const lst &sym_lst, string &name, unsigned int index) const
{
	if (index >= exprs.size())
		throw (std::range_error("index of archived expression out of range"));

	// Return expression name
	name = unatomize(exprs[index].name);

	// Recursively unarchive all nodes, starting at the root node
	return nodes[exprs[index].root].unarchive(sym_lst);
}


/** Return number of archived expressions. */
unsigned int archive::num_expressions(void) const
{
	return exprs.size();
}


/*
 *  Archive file format
 *
 *   - 4 bytes signature 'GARC'
 *   - unsigned version number
 *   - unsigned number of atoms
 *      - atom strings (each zero-terminated)
 *   - unsigned number of expressions
 *      - unsigned name atom
 *      - unsigned root node ID
 *   - unsigned number of nodes
 *      - unsigned number of properties
 *        - unsigned containing type (PTYPE_*) in its lower 3 bits and
 *          name atom in the upper bits
 *        - unsigned property value
 *
 *  Unsigned quantities are stored in a compressed format:
 *   - numbers in the range 0x00..0x7f are stored verbatim (1 byte)
 *   - numbers larger than 0x7f are stored in 7-bit packets (1 byte per
 *     packet), starting with the LSBs; all bytes except the last one have
 *     their upper bit set
 *
 *  Examples:
 *   0x00           =   0x00
 *    ..                 ..
 *   0x7f           =   0x7f
 *   0x80 0x01      =   0x80
 *    ..   ..            ..
 *   0xff 0x01      =   0xff
 *   0x80 0x02      =  0x100
 *    ..   ..            ..
 *   0xff 0x02      =  0x17f
 *   0x80 0x03      =  0x180
 *    ..   ..            ..
 *   0xff 0x7f      = 0x3fff
 *   0x80 0x80 0x01 = 0x4000
 *    ..   ..   ..       ..
 */

/** Write unsigned integer quantity to stream. */
static void write_unsigned(ostream &os, unsigned int val)
{
	while (val > 0x80) {
		os.put((val & 0x7f) | 0x80);
		val >>= 7;
	}
	os.put(val);
}

/** Read unsigned integer quantity from stream. */
static unsigned int read_unsigned(istream &is)
{
	unsigned char b;
	unsigned int ret = 0;
	unsigned int shift = 0;
	do {
		is.get(b);
		ret |= (b & 0x7f) << shift;
		shift += 7;
	} while (b & 0x80);
	return ret;
}

/** Write archive_node to binary data stream. */
ostream &operator<<(ostream &os, const archive_node &n)
{
	// Write properties
	unsigned int num_props = n.props.size();
	write_unsigned(os, num_props);
	for (unsigned int i=0; i<num_props; i++) {
		write_unsigned(os, n.props[i].type | (n.props[i].name << 3));
		write_unsigned(os, n.props[i].value);
	}
    return os;
}

/** Write archive to binary data stream. */
ostream &operator<<(ostream &os, const archive &ar)
{
	// Write header
	os.put('G');	// Signature
	os.put('A');
	os.put('R');
	os.put('C');
	write_unsigned(os, ARCHIVE_VERSION);

	// Write atoms
	unsigned int num_atoms = ar.atoms.size();
	write_unsigned(os, num_atoms);
	for (unsigned int i=0; i<num_atoms; i++)
		os << ar.atoms[i] << ends;

	// Write expressions
	unsigned int num_exprs = ar.exprs.size();
	write_unsigned(os, num_exprs);
	for (unsigned int i=0; i<num_exprs; i++) {
		write_unsigned(os, ar.exprs[i].name);
		write_unsigned(os, ar.exprs[i].root);
	}

	// Write nodes
	unsigned int num_nodes = ar.nodes.size();
	write_unsigned(os, num_nodes);
	for (unsigned int i=0; i<num_nodes; i++)
		os << ar.nodes[i];
    return os;
}

/** Read archive_node from binary data stream. */
istream &operator>>(istream &is, archive_node &n)
{
	// Read properties
	unsigned int num_props = read_unsigned(is);
	n.props.resize(num_props);
	for (unsigned int i=0; i<num_props; i++) {
		unsigned int name_type = read_unsigned(is);
		n.props[i].type = (archive_node::property_type)(name_type & 7);
		n.props[i].name = name_type >> 3;
		n.props[i].value = read_unsigned(is);
	}
    return is;
}

/** Read archive from binary data stream. */
istream &operator>>(istream &is, archive &ar)
{
	// Read header
	char c1, c2, c3, c4;
	is.get(c1); is.get(c2); is.get(c3); is.get(c4);
	if (c1 != 'G' || c2 != 'A' || c3 != 'R' || c4 != 'C')
		throw (std::runtime_error("not a GiNaC archive (signature not found)"));
	unsigned int version = read_unsigned(is);
	if (version > ARCHIVE_VERSION || version < ARCHIVE_VERSION - ARCHIVE_AGE)
		throw (std::runtime_error("archive version " + ToString(version) + " cannot be read by this GiNaC library (which supports versions " + ToString(ARCHIVE_VERSION-ARCHIVE_AGE) + " thru " + ToString(ARCHIVE_VERSION)));

	// Read atoms
	unsigned int num_atoms = read_unsigned(is);
	ar.atoms.resize(num_atoms);
	for (unsigned int i=0; i<num_atoms; i++)
		getline(is, ar.atoms[i], '\0');

	// Read expressions
	unsigned int num_exprs = read_unsigned(is);
	ar.exprs.resize(num_exprs);
	for (unsigned int i=0; i<num_exprs; i++) {
		archive_atom name = read_unsigned(is);
		archive_node_id root = read_unsigned(is);
		ar.exprs[i] = archive::archived_ex(name, root);
	}

	// Read nodes
	unsigned int num_nodes = read_unsigned(is);
	ar.nodes.resize(num_nodes, ar);
	for (unsigned int i=0; i<num_nodes; i++)
		is >> ar.nodes[i];
    return is;
}


/** Atomize a string (i.e. convert it into an ID number that uniquely
 *  represents the string). */
archive_atom archive::atomize(const string &s) const
{
	// Search for string in atoms vector
	vector<string>::const_iterator i = atoms.begin(), iend = atoms.end();
	archive_atom id = 0;
	while (i != iend) {
		if (*i == s)
			return id;
		i++; id++;
	}

	// Not found, add to atoms vector
	atoms.push_back(s);
	return id;
}

/** Unatomize a string (i.e. convert the ID number back to the string). */
const string &archive::unatomize(archive_atom id) const
{
	if (id >= atoms.size())
		throw (std::range_error("archive::unatomizee(): atom ID out of range"));

	return atoms[id];
}


/** Copy constructor of archive_node. */
archive_node::archive_node(const archive_node &other)
	: a(other.a), props(other.props), has_expression(other.has_expression), e(other.e)
{
}


/** Assignment operator of archive_node. */
const archive_node &archive_node::operator=(const archive_node &other)
{
	if (this != &other) {
		a = other.a;
		props = other.props;
		has_expression = other.has_expression;
		e = other.e;
	}
	return *this;
}


/** Recursively construct archive node from expression. */
archive_node::archive_node(archive &ar, const ex &expr)
	: a(ar), has_expression(true), e(expr)
{
	expr.bp->archive(*this);
}


/** Check if the archive_node stores the same expression as another
 *  archive_node.
 *  @return "true" if expressions are the same */
bool archive_node::has_same_ex_as(const archive_node &other) const
{
	if (!has_expression || !other.has_expression)
		return false;
	return e.bp == other.e.bp;
}


/** Add property of type "bool" to node. */
void archive_node::add_bool(const string &name, bool value)
{
	props.push_back(property(a.atomize(name), PTYPE_BOOL, value));
}

/** Add property of type "unsigned int" to node. */
void archive_node::add_unsigned(const string &name, unsigned int value)
{
	props.push_back(property(a.atomize(name), PTYPE_UNSIGNED, value));
}

/** Add property of type "string" to node. */
void archive_node::add_string(const string &name, const string &value)
{
	props.push_back(property(a.atomize(name), PTYPE_STRING, a.atomize(value)));
}

/** Add property of type "ex" to node. */
void archive_node::add_ex(const string &name, const ex &value)
{
	// Recursively create an archive_node and add its ID to the properties of this node
	archive_node_id id = a.add_node(archive_node(a, value));
	props.push_back(property(a.atomize(name), PTYPE_NODE, id));
}


/** Retrieve property of type "bool" from node.
 *  @return "true" if property was found, "false" otherwise */
bool archive_node::find_bool(const string &name, bool &ret) const
{
	archive_atom name_atom = a.atomize(name);
	vector<property>::const_iterator i = props.begin(), iend = props.end();
	while (i != iend) {
		if (i->type == PTYPE_BOOL && i->name == name_atom) {
			ret = i->value;
			return true;
		}
		i++;
	}
	return false;
}

/** Retrieve property of type "unsigned" from node.
 *  @return "true" if property was found, "false" otherwise */
bool archive_node::find_unsigned(const string &name, unsigned int &ret) const
{
	archive_atom name_atom = a.atomize(name);
	vector<property>::const_iterator i = props.begin(), iend = props.end();
	while (i != iend) {
		if (i->type == PTYPE_UNSIGNED && i->name == name_atom) {
			ret = i->value;
			return true;
		}
		i++;
	}
	return false;
}

/** Retrieve property of type "string" from node.
 *  @return "true" if property was found, "false" otherwise */
bool archive_node::find_string(const string &name, string &ret) const
{
	archive_atom name_atom = a.atomize(name);
	vector<property>::const_iterator i = props.begin(), iend = props.end();
	while (i != iend) {
		if (i->type == PTYPE_STRING && i->name == name_atom) {
			ret = a.unatomize(i->value);
			return true;
		}
		i++;
	}
	return false;
}

/** Retrieve property of type "ex" from node.
 *  @return "true" if property was found, "false" otherwise */
bool archive_node::find_ex(const string &name, ex &ret, const lst &sym_lst, unsigned int index) const
{
	archive_atom name_atom = a.atomize(name);
	vector<property>::const_iterator i = props.begin(), iend = props.end();
	unsigned int found_index = 0;
	while (i != iend) {
		if (i->type == PTYPE_NODE && i->name == name_atom) {
			if (found_index == index)
				goto found;
			found_index++;
		}
		i++;
	}
	return false;

found:
	ret = a.get_node(i->value).unarchive(sym_lst);
	return true;
}


/** Convert archive node to GiNaC expression. */
ex archive_node::unarchive(const lst &sym_lst) const
{
	// Already unarchived? Then return cached unarchived expression.
	if (has_expression)
		return e;

	// Find instantiation function for class specified in node
	string class_name;
	if (!find_string("class", class_name))
		throw (std::runtime_error("archive node contains no class name"));
	unarch_func f = find_unarch_func(class_name);

	// Call instantiation function
	e = f(*this, sym_lst);
	has_expression = true;
	return e;
}


/** Assignment operator of property. */
const archive_node::property &archive_node::property::operator=(const property &other)
{
	if (this != &other) {
		type = other.type;
		name = other.name;
		value = other.value;
	}
	return *this;
}


/** Clear all archived expressions. */
void archive::clear(void)
{
	atoms.clear();
	exprs.clear();
	nodes.clear();
}


/** Delete cached unarchived expressions in all archive_nodes (mainly for debugging). */
void archive::forget(void)
{
	vector<archive_node>::iterator i = nodes.begin(), iend = nodes.end();
	while (i != iend) {
		i->forget();
		i++;
	}
}

/** Delete cached unarchived expressions from node (for debugging). */
void archive_node::forget(void)
{
	has_expression = false;
	e = 0;
}


/** Print archive to stream in ugly raw format (for debugging). */
void archive::printraw(ostream &os) const
{
	// Dump atoms
	os << "Atoms:\n";
	{
		vector<string>::const_iterator i = atoms.begin(), iend = atoms.end();
		archive_atom id = 0;
		while (i != iend) {
			os << " " << id << " " << *i << endl;
			i++; id++;
		}
	}
	os << endl;

	// Dump expressions
	os << "Expressions:\n";
	{
		vector<archived_ex>::const_iterator i = exprs.begin(), iend = exprs.end();
		unsigned int index = 0;
		while (i != iend) {
			os << " " << index << " \"" << unatomize(i->name) << "\" root node " << i->root << endl;
			i++; index++;
		}
	}
	os << endl;

	// Dump nodes
	os << "Nodes:\n";
	{
		vector<archive_node>::const_iterator i = nodes.begin(), iend = nodes.end();
		archive_node_id id = 0;
		while (i != iend) {
			os << " " << id << " ";
			i->printraw(os);
			i++; id++;
		}
	}
}

/** Output archive_node to stream in ugly raw format (for debugging). */
void archive_node::printraw(ostream &os) const
{
	// Dump cached unarchived expression
	if (has_expression)
		os << "(basic * " << e.bp << " = " << e << ")\n";
	else
		os << "\n";

	// Dump properties
	vector<property>::const_iterator i = props.begin(), iend = props.end();
	while (i != iend) {
		os << "  ";
		switch (i->type) {
			case PTYPE_BOOL: os << "bool"; break;
			case PTYPE_UNSIGNED: os << "unsigned"; break;
			case PTYPE_STRING: os << "string"; break;
			case PTYPE_NODE: os << "node"; break;
			default: os << "<unknown>"; break;
		}
		os << " \"" << a.unatomize(i->name) << "\" " << i->value << endl;
		i++;
	}
}

/** Create a dummy archive.  The intention is to fill archive_node's default ctor,
 *  which is currently a Cint-requirement. */
archive* archive_node::dummy_ar_creator(void)
{
    static archive* some_ar = new archive;
    return some_ar;
}


#ifndef NO_NAMESPACE_GINAC
} // namespace GiNaC
#endif // ndef NO_NAMESPACE_GINAC