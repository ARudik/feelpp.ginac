/** @file clifford.cpp
 *
 *  Implementation of GiNaC's clifford algebra (Dirac gamma) objects. */

/*
 *  GiNaC Copyright (C) 1999-2001 Johannes Gutenberg University Mainz, Germany
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

#include "clifford.h"
#include "ex.h"
#include "idx.h"
#include "ncmul.h"
#include "symbol.h"
#include "numeric.h" // for I
#include "symmetry.h"
#include "lst.h"
#include "relational.h"
#include "print.h"
#include "archive.h"
#include "debugmsg.h"
#include "utils.h"

#include <stdexcept>

namespace GiNaC {

GINAC_IMPLEMENT_REGISTERED_CLASS(clifford, indexed)
GINAC_IMPLEMENT_REGISTERED_CLASS(diracone, tensor)
GINAC_IMPLEMENT_REGISTERED_CLASS(diracgamma, tensor)
GINAC_IMPLEMENT_REGISTERED_CLASS(diracgamma5, tensor)

//////////
// default constructor, destructor, copy constructor assignment operator and helpers
//////////

clifford::clifford() : representation_label(0)
{
	debugmsg("clifford default constructor", LOGLEVEL_CONSTRUCT);
	tinfo_key = TINFO_clifford;
}

void clifford::copy(const clifford & other)
{
	inherited::copy(other);
	representation_label = other.representation_label;
}

DEFAULT_DESTROY(clifford)
DEFAULT_CTORS(diracone)
DEFAULT_CTORS(diracgamma)
DEFAULT_CTORS(diracgamma5)

//////////
// other constructors
//////////

/** Construct object without any indices. This constructor is for internal
 *  use only. Use the dirac_ONE() function instead.
 *  @see dirac_ONE */
clifford::clifford(const ex & b, unsigned char rl) : inherited(b), representation_label(rl)
{
	debugmsg("clifford constructor from ex", LOGLEVEL_CONSTRUCT);
	tinfo_key = TINFO_clifford;
}

/** Construct object with one Lorentz index. This constructor is for internal
 *  use only. Use the dirac_gamma() function instead.
 *  @see dirac_gamma */
clifford::clifford(const ex & b, const ex & mu, unsigned char rl) : inherited(b, mu), representation_label(rl)
{
	debugmsg("clifford constructor from ex,ex", LOGLEVEL_CONSTRUCT);
	GINAC_ASSERT(is_ex_of_type(mu, varidx));
	tinfo_key = TINFO_clifford;
}

clifford::clifford(unsigned char rl, const exvector & v, bool discardable) : inherited(sy_none(), v, discardable), representation_label(rl)
{
	debugmsg("clifford constructor from unsigned char,exvector", LOGLEVEL_CONSTRUCT);
	tinfo_key = TINFO_clifford;
}

clifford::clifford(unsigned char rl, exvector * vp) : inherited(sy_none(), vp), representation_label(rl)
{
	debugmsg("clifford constructor from unsigned char,exvector *", LOGLEVEL_CONSTRUCT);
	tinfo_key = TINFO_clifford;
}

//////////
// archiving
//////////

clifford::clifford(const archive_node &n, const lst &sym_lst) : inherited(n, sym_lst)
{
	debugmsg("clifford constructor from archive_node", LOGLEVEL_CONSTRUCT);
	unsigned rl;
	n.find_unsigned("label", rl);
	representation_label = rl;
}

void clifford::archive(archive_node &n) const
{
	inherited::archive(n);
	n.add_unsigned("label", representation_label);
}

DEFAULT_UNARCHIVE(clifford)
DEFAULT_ARCHIVING(diracone)
DEFAULT_ARCHIVING(diracgamma)
DEFAULT_ARCHIVING(diracgamma5)

//////////
// functions overriding virtual functions from bases classes
//////////

int clifford::compare_same_type(const basic & other) const
{
	GINAC_ASSERT(other.tinfo() == TINFO_clifford);
	const clifford &o = static_cast<const clifford &>(other);

	if (representation_label != o.representation_label) {
		// different representation label
		return representation_label < o.representation_label ? -1 : 1;
	}

	return inherited::compare_same_type(other);
}

DEFAULT_COMPARE(diracone)
DEFAULT_COMPARE(diracgamma)
DEFAULT_COMPARE(diracgamma5)

DEFAULT_PRINT_LATEX(diracone, "ONE", "\\mathbb{1}")
DEFAULT_PRINT_LATEX(diracgamma, "gamma", "\\gamma")
DEFAULT_PRINT_LATEX(diracgamma5, "gamma5", "{\\gamma^5}")

/** Contraction of a gamma matrix with something else. */
bool diracgamma::contract_with(exvector::iterator self, exvector::iterator other, exvector & v) const
{
	GINAC_ASSERT(is_ex_of_type(*self, clifford));
	GINAC_ASSERT(is_ex_of_type(*other, indexed));
	GINAC_ASSERT(is_ex_of_type(self->op(0), diracgamma));
	unsigned char rl = ex_to<clifford>(*self).get_representation_label();

	if (is_ex_of_type(*other, clifford)) {

		ex dim = ex_to<idx>(self->op(1)).get_dim();

		// gamma~mu gamma.mu = dim ONE
		if (other - self == 1) {
			*self = dim;
			*other = dirac_ONE(rl);
			return true;

		// gamma~mu gamma~alpha gamma.mu = (2-dim) gamma~alpha
		} else if (other - self == 2
		        && is_ex_of_type(self[1], clifford)) {
			*self = 2 - dim;
			*other = _ex1();
			return true;

		// gamma~mu gamma~alpha gamma~beta gamma.mu = 4 g~alpha~beta + (dim-4) gamam~alpha gamma~beta
		} else if (other - self == 3
		        && is_ex_of_type(self[1], clifford)
		        && is_ex_of_type(self[2], clifford)) {
			*self = 4 * lorentz_g(self[1].op(1), self[2].op(1)) * dirac_ONE(rl) + (dim - 4) * self[1] * self[2];
			self[1] = _ex1();
			self[2] = _ex1();
			*other = _ex1();
			return true;

		// gamma~mu S gamma~alpha gamma.mu = 2 gamma~alpha S - gamma~mu S gamma.mu gamma~alpha
		// (commutate contracted indices towards each other, simplify_indexed()
		// will re-expand and re-run the simplification)
		} else {
			exvector::iterator it = self + 1, next_to_last = other - 1;
			while (it != other) {
				if (!is_ex_of_type(*it, clifford))
					return false;
				it++;
			}

			it = self + 1;
			ex S = _ex1();
			while (it != next_to_last) {
				S *= *it;
				*it++ = _ex1();
			}

			*self = 2 * (*next_to_last) * S - (*self) * S * (*other) * (*next_to_last);
			*next_to_last = _ex1();
			*other = _ex1();
			return true;
		}
	}

	return false;
}

/** Perform automatic simplification on noncommutative product of clifford
 *  objects. This removes superfluous ONEs, permutes gamma5's to the front
 *  and removes squares of gamma objects. */
ex clifford::simplify_ncmul(const exvector & v) const
{
	exvector s;
	s.reserve(v.size());

	// Remove superfluous ONEs
	exvector::const_iterator cit = v.begin(), citend = v.end();
	while (cit != citend) {
		if (!is_ex_of_type(cit->op(0), diracone))
			s.push_back(*cit);
		cit++;
	}

	bool something_changed = false;
	int sign = 1;

	// Anticommute gamma5's to the front
	if (s.size() >= 2) {
		exvector::iterator first = s.begin(), next_to_last = s.end() - 2;
		while (true) {
			exvector::iterator it = next_to_last;
			while (true) {
				exvector::iterator it2 = it + 1;
				if (!is_ex_of_type(it->op(0), diracgamma5) && is_ex_of_type(it2->op(0), diracgamma5)) {
					it->swap(*it2);
					sign = -sign;
					something_changed = true;
				}
				if (it == first)
					break;
				it--;
			}
			if (next_to_last == first)
				break;
			next_to_last--;
		}
	}

	// Remove squares of gamma5
	while (s.size() >= 2 && is_ex_of_type(s[0].op(0), diracgamma5) && is_ex_of_type(s[1].op(0), diracgamma5)) {
		s.erase(s.begin(), s.begin() + 2);
		something_changed = true;
	}

	// Remove equal adjacent gammas
	if (s.size() >= 2) {
		exvector::iterator it = s.begin(), itend = s.end() - 1;
		while (it != itend) {
			ex & a = it[0];
			ex & b = it[1];
			if (is_ex_of_type(a.op(0), diracgamma) && is_ex_of_type(b.op(0), diracgamma)) {
				const ex & ia = a.op(1);
				const ex & ib = b.op(1);
				if (ia.is_equal(ib)) {
					a = lorentz_g(ia, ib);
					b = dirac_ONE(representation_label);
					something_changed = true;
				}
			}
			it++;
		}
	}

	if (s.size() == 0)
		return clifford(diracone(), representation_label) * sign;
	if (something_changed)
		return nonsimplified_ncmul(s) * sign;
	else
		return simplified_ncmul(s) * sign;
}

ex clifford::thisexprseq(const exvector & v) const
{
	return clifford(representation_label, v);
}

ex clifford::thisexprseq(exvector * vp) const
{
	return clifford(representation_label, vp);
}

//////////
// global functions
//////////

ex dirac_ONE(unsigned char rl)
{
	return clifford(diracone(), rl);
}

ex dirac_gamma(const ex & mu, unsigned char rl)
{
	if (!is_ex_of_type(mu, varidx))
		throw(std::invalid_argument("index of Dirac gamma must be of type varidx"));

	return clifford(diracgamma(), mu, rl);
}

ex dirac_gamma5(unsigned char rl)
{
	return clifford(diracgamma5(), rl);
}

ex dirac_gamma6(unsigned char rl)
{
	return clifford(diracone(), rl) + clifford(diracgamma5(), rl);
}

ex dirac_gamma7(unsigned char rl)
{
	return clifford(diracone(), rl) - clifford(diracgamma5(), rl);
}

ex dirac_slash(const ex & e, const ex & dim, unsigned char rl)
{
	varidx mu((new symbol)->setflag(status_flags::dynallocated), dim);
	return indexed(e, mu.toggle_variance()) * dirac_gamma(mu, rl);
}

/** Check whether a given tinfo key (as returned by return_type_tinfo()
 *  is that of a clifford object with the specified representation label. */
static bool is_clifford_tinfo(unsigned ti, unsigned char rl)
{
	return ti == (TINFO_clifford + rl);
}

/** Check whether a given tinfo key (as returned by return_type_tinfo()
 *  is that of a clifford object (with an arbitrary representation label). */
static bool is_clifford_tinfo(unsigned ti)
{
	return (ti & ~0xff) == TINFO_clifford;
}

/** Take trace of a string of an even number of Dirac gammas given a vector
 *  of indices. */
static ex trace_string(exvector::const_iterator ix, unsigned num)
{
	// Tr gamma.mu gamma.nu = 4 g.mu.nu
	if (num == 2)
		return lorentz_g(ix[0], ix[1]);

	// Tr gamma.mu gamma.nu gamma.rho gamma.sig = 4 (g.mu.nu g.rho.sig + g.nu.rho g.mu.sig - g.mu.rho g.nu.sig
	else if (num == 4)
		return lorentz_g(ix[0], ix[1]) * lorentz_g(ix[2], ix[3])
		     + lorentz_g(ix[1], ix[2]) * lorentz_g(ix[0], ix[3])
		     - lorentz_g(ix[0], ix[2]) * lorentz_g(ix[1], ix[3]);

	// Traces of 6 or more gammas are computed recursively:
	// Tr gamma.mu1 gamma.mu2 ... gamma.mun =
	//   + g.mu1.mu2 * Tr gamma.mu3 ... gamma.mun
	//   - g.mu1.mu3 * Tr gamma.mu2 gamma.mu4 ... gamma.mun
	//   + g.mu1.mu4 * Tr gamma.mu3 gamma.mu3 gamma.mu5 ... gamma.mun
	//   - ...
	//   + g.mu1.mun * Tr gamma.mu2 ... gamma.mu(n-1)
	exvector v(num - 2);
	int sign = 1;
	ex result;
	for (int i=1; i<num; i++) {
		for (int n=1, j=0; n<num; n++) {
			if (n == i)
				continue;
			v[j++] = ix[n];
		}
		result += sign * lorentz_g(ix[0], ix[i]) * trace_string(v.begin(), num-2);
		sign = -sign;
	}
	return result;
}

ex dirac_trace(const ex & e, unsigned char rl, const ex & trONE)
{
	if (is_ex_of_type(e, clifford)) {

		if (ex_to<clifford>(e).get_representation_label() == rl
		 && is_ex_of_type(e.op(0), diracone))
			return trONE;
		else
			return _ex0();

	} else if (is_ex_exactly_of_type(e, mul)) {

		// Trace of product: pull out non-clifford factors
		ex prod = _ex1();
		for (unsigned i=0; i<e.nops(); i++) {
			const ex &o = e.op(i);
			unsigned ti = o.return_type_tinfo();
			if (is_clifford_tinfo(o.return_type_tinfo(), rl))
				prod *= dirac_trace(o, rl, trONE);
			else
				prod *= o;
		}
		return prod;

	} else if (is_ex_exactly_of_type(e, ncmul)) {

		if (!is_clifford_tinfo(e.return_type_tinfo(), rl))
			return _ex0();

		// Expand product, if necessary
		ex e_expanded = e.expand();
		if (!is_ex_of_type(e_expanded, ncmul))
			return dirac_trace(e_expanded, rl, trONE);

		// gamma5 gets moved to the front so this check is enough
		bool has_gamma5 = is_ex_of_type(e.op(0).op(0), diracgamma5);
		unsigned num = e.nops();

		if (has_gamma5) {

			// Trace of gamma5 * odd number of gammas and trace of
			// gamma5 * gamma.mu * gamma.nu are zero
			if ((num & 1) == 0 || num == 3)
				return _ex0();

			// Tr gamma5 gamma.mu gamma.nu gamma.rho gamma.sigma = 4I * epsilon(mu, nu, rho, sigma)
			if (num == 5)
				return trONE * I * eps0123(e.op(1).op(1), e.op(2).op(1), e.op(3).op(1), e.op(4).op(1));

	   		// Tr gamma5 S_2k =
			//   I/4! * epsilon0123.mu1.mu2.mu3.mu4 * Tr gamma.mu1 gamma.mu2 gamma.mu3 gamma.mu4 S_2k
			exvector ix;
			ix.reserve(num - 1);
			for (unsigned i=1; i<num; i++)
				ix.push_back(e.op(i).op(1));
			num--;
			int *iv = new int[num];
			ex result;
			for (int i=0; i<num-3; i++) {
				ex idx1 = ix[i];
				for (int j=i+1; j<num-2; j++) {
					ex idx2 = ix[j];
					for (int k=j+1; k<num-1; k++) {
						ex idx3 = ix[k];
						for (int l=k+1; l<num; l++) {
							ex idx4 = ix[l];
							iv[0] = i; iv[1] = j; iv[2] = k; iv[3] = l;
							exvector v;
							v.reserve(num - 4);
							for (int n=0, t=4; n<num; n++) {
								if (n == i || n == j || n == k || n == l)
									continue;
								iv[t++] = n;
								v.push_back(ix[n]);
							}
							int sign = permutation_sign(iv, iv + num);
							result += sign * eps0123(idx1, idx2, idx3, idx4)
							        * trace_string(v.begin(), num - 4);
						}
					}
				}
			}
			delete[] iv;
			return trONE * I * result;

		} else { // no gamma5

			// Trace of odd number of gammas is zero
			if ((num & 1) == 1)
				return _ex0();

			// Tr gamma.mu gamma.nu = 4 g.mu.nu
			if (num == 2)
				return trONE * lorentz_g(e.op(0).op(1), e.op(1).op(1));

			exvector iv;
			iv.reserve(num);
			for (unsigned i=0; i<num; i++)
				iv.push_back(e.op(i).op(1));

			return trONE * trace_string(iv.begin(), num);
		}

	} else if (e.nops() > 0) {

		// Trace maps to all other container classes (this includes sums)
		pointer_to_map_function_2args<unsigned char, const ex &> fcn(dirac_trace, rl, trONE);
		return e.map(fcn);

	} else
		return _ex0();
}

ex canonicalize_clifford(const ex & e)
{
	// Scan for any ncmul objects
	lst srl;
	ex aux = e.to_rational(srl);
	for (unsigned i=0; i<srl.nops(); i++) {

		ex lhs = srl.op(i).lhs();
		ex rhs = srl.op(i).rhs();

		if (is_ex_exactly_of_type(rhs, ncmul)
		 && rhs.return_type() == return_types::noncommutative
		 && is_clifford_tinfo(rhs.return_type_tinfo())) {

			// Expand product, if necessary
			ex rhs_expanded = rhs.expand();
			if (!is_ex_of_type(rhs_expanded, ncmul)) {
				srl.let_op(i) = (lhs == canonicalize_clifford(rhs_expanded));
				continue;

			} else if (!is_ex_of_type(rhs.op(0), clifford))
				continue;

			exvector v;
			v.reserve(rhs.nops());
			for (unsigned j=0; j<rhs.nops(); j++)
				v.push_back(rhs.op(j));

			// Stupid recursive bubble sort because we only want to swap adjacent gammas
			exvector::iterator it = v.begin(), next_to_last = v.end() - 1;
			if (is_ex_of_type(it->op(0), diracgamma5))
				it++;
			while (it != next_to_last) {
				if (it[0].op(1).compare(it[1].op(1)) > 0) {
					ex save0 = it[0], save1 = it[1];
					it[0] = lorentz_g(it[0].op(1), it[1].op(1));
					it[1] = _ex2();
					ex sum = ncmul(v);
					it[0] = save1;
					it[1] = save0;
					sum -= ncmul(v, true);
					srl.let_op(i) = (lhs == canonicalize_clifford(sum));
					goto next_sym;
				}
				it++;
			}
next_sym:	;
		}
	}
	return aux.subs(srl);
}

} // namespace GiNaC
