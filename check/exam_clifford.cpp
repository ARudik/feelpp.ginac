/** @file exam_clifford.cpp
 *
 *  Here we test GiNaC's Clifford algebra objects. */

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

#include "exams.h"

static unsigned check_equal(const ex &e1, const ex &e2)
{
	ex e = e1 - e2;
	if (!e.is_zero()) {
		clog << e1 << "-" << e2 << " erroneously returned "
		     << e << " instead of 0" << endl;
		return 1;
	}
	return 0;
}

static unsigned check_equal_simplify(const ex &e1, const ex &e2)
{
	ex e = simplify_indexed(e1) - e2;
	if (!e.is_zero()) {
		clog << "simplify_indexed(" << e1 << ")-" << e2 << " erroneously returned "
		     << e << " instead of 0" << endl;
		return 1;
	}
	return 0;
}

static unsigned clifford_check1(void)
{
	// checks general identities and contractions

	unsigned result = 0;

	symbol dim("D");
	varidx mu(symbol("mu"), dim), nu(symbol("nu"), dim);
	ex e;

	e = dirac_ONE() * dirac_ONE();
	result += check_equal(e, dirac_ONE());

	e = dirac_ONE() * dirac_gamma(mu) * dirac_ONE();
	result += check_equal(e, dirac_gamma(mu));

	e = dirac_gamma(varidx(2, dim)) * dirac_gamma(varidx(1, dim)) *
	    dirac_gamma(varidx(1, dim)) * dirac_gamma(varidx(2, dim));
	result += check_equal(e, dirac_ONE());

	e = dirac_gamma(mu) * dirac_gamma(nu) *
	    dirac_gamma(nu.toggle_variance()) * dirac_gamma(mu.toggle_variance());
	result += check_equal_simplify(e, pow(dim, 2) * dirac_ONE());

	e = dirac_gamma(mu) * dirac_gamma(nu) *
	    dirac_gamma(mu.toggle_variance()) * dirac_gamma(nu.toggle_variance());
	result += check_equal_simplify(e, 2*dim*dirac_ONE()-pow(dim, 2)*dirac_ONE());

	return result;
}

static unsigned clifford_check2(void)
{
	// checks identities relating to gamma5

	unsigned result = 0;

	symbol dim("D");
	varidx mu(symbol("mu"), dim), nu(symbol("nu"), dim);
	ex e;

	e = dirac_gamma(mu) * dirac_gamma5() + dirac_gamma5() * dirac_gamma(mu);
	result += check_equal(e, 0);

	e = dirac_gamma5() * dirac_gamma(mu) * dirac_gamma5() + dirac_gamma(mu);
	result += check_equal(e, 0);

	return result;
}

static unsigned clifford_check3(void)
{
	// checks traces

	unsigned result = 0;

	symbol dim("D"), m("m"), q("q"), l("l"), ldotq("ldotq");
	varidx mu(symbol("mu"), dim), nu(symbol("nu"), dim), rho(symbol("rho"), dim),
	       sig(symbol("sig"), dim), kap(symbol("kap"), dim), lam(symbol("lam"), 4);
	ex e;

	e = dirac_gamma(mu);
	result += check_equal(dirac_trace(e), 0);

	e = dirac_gamma(mu) * dirac_gamma(nu) * dirac_gamma(rho);
	result += check_equal(dirac_trace(e), 0);

	e = dirac_gamma5() * dirac_gamma(mu);
	result += check_equal(dirac_trace(e), 0);

	e = dirac_gamma5() * dirac_gamma(mu) * dirac_gamma(nu);
	result += check_equal(dirac_trace(e), 0);

	e = dirac_gamma5() * dirac_gamma(mu) * dirac_gamma(nu) * dirac_gamma(rho);
	result += check_equal(dirac_trace(e), 0);

	scalar_products sp;
	sp.add(q, q, pow(q, 2));
	sp.add(l, l, pow(l, 2));
	sp.add(l, q, ldotq);

	e = pow(m, 2) * dirac_slash(q, dim) * dirac_slash(q, dim);
	e = dirac_trace(e).simplify_indexed(sp);
	result += check_equal(e, 4*pow(m, 2)*pow(q, 2));

	// cyclicity without gamma5
	e = dirac_gamma(mu) * dirac_gamma(nu) * dirac_gamma(rho) * dirac_gamma(sig)
	  - dirac_gamma(nu) * dirac_gamma(rho) * dirac_gamma(sig) * dirac_gamma(mu);
	e = dirac_trace(e);
	result += check_equal(e, 0);

	e = dirac_gamma(mu) * dirac_gamma(nu) * dirac_gamma(rho) * dirac_gamma(sig) * dirac_gamma(kap) * dirac_gamma(lam)
	  - dirac_gamma(nu) * dirac_gamma(rho) * dirac_gamma(sig) * dirac_gamma(kap) * dirac_gamma(lam) * dirac_gamma(mu);
	e = dirac_trace(e).expand();
	result += check_equal(e, 0);

	// cyclicity of gamma5 * S_4
	e = dirac_gamma5() * dirac_gamma(mu) * dirac_gamma(nu) * dirac_gamma(rho) * dirac_gamma(sig)
	  - dirac_gamma(sig) * dirac_gamma5() * dirac_gamma(mu) * dirac_gamma(nu) * dirac_gamma(rho);
	e = dirac_trace(e);
	result += check_equal(e, 0);

	// non-cyclicity of order D-4 of gamma5 * S_6
	e = dirac_gamma5() * dirac_gamma(mu) * dirac_gamma(nu) * dirac_gamma(rho) * dirac_gamma(sig) * dirac_gamma(kap) * dirac_gamma(mu.toggle_variance())
	  + dim * dirac_gamma5() * dirac_gamma(nu) * dirac_gamma(rho) * dirac_gamma(sig) * dirac_gamma(kap);
	e = dirac_trace(e);
	e = (e / (dim - 4)).normal();
	result += check_equal(e, 8 * eps0123(nu, rho, sig, kap));

	// one-loop vacuum polarization in QED
	e = dirac_gamma(mu) *
	    (dirac_slash(l, dim) + dirac_slash(q, dim) + m * dirac_ONE()) *
	    dirac_gamma(mu.toggle_variance()) *
	    (dirac_slash(l, dim) + m * dirac_ONE());
	e = dirac_trace(e).simplify_indexed(sp);
	result += check_equal(e, 4*((2-dim)*l*l + (2-dim)*ldotq + dim*m*m).expand());

	e = dirac_slash(q, dim) *
	    (dirac_slash(l, dim) + dirac_slash(q, dim) + m * dirac_ONE()) *
	    dirac_slash(q, dim) *
	    (dirac_slash(l, dim) + m * dirac_ONE());
	e = dirac_trace(e).simplify_indexed(sp);
	result += check_equal(e, 4*(2*ldotq*ldotq + q*q*ldotq - q*q*l*l + q*q*m*m).expand());

	return result;
}

unsigned exam_clifford(void)
{
	unsigned result = 0;
	
	cout << "examining clifford objects" << flush;
	clog << "----------clifford objects:" << endl;

	result += clifford_check1();  cout << '.' << flush;
	result += clifford_check2();  cout << '.' << flush;
	result += clifford_check3();  cout << '.' << flush;
	
	if (!result) {
		cout << " passed " << endl;
		clog << "(no output)" << endl;
	} else {
		cout << " failed " << endl;
	}
	
	return result;
}
