
#include "Cleanup.hh"
#include "Functional.hh"
#include "algorithms/factor_out.hh"
#include <map>

factor_out::factor_out(const Kernel& k, Ex& e, Ex& args)
	: Algorithm(k, e)
	{
	cadabra::do_list(args, args.begin(), [&](Ex::iterator arg) {
			to_factor_out.push_back(Ex(arg));
			return true;
			}
		);
	}

/// Check if the expression is a sum with more than one term
bool factor_out::can_apply(iterator st)
	{
	if(*st->name=="\\sum")  return true;
	return false;
	}

Algorithm::result_t factor_out::apply(iterator& it)
	{
	result_t result=result_t::l_no_action;

	// For every term in the sum, we look at the factors in the product
	// (or at the single object if there is no product). If this factor
	// needs to be factored out, we determine if it can be moved all the
	// way to the left of the expression. If so, move the object to
	// a 'factored_out' temporary, and take out of the tree. Rinse/repeat.
	// What's left at the end is two objects: the stuff factored out,
	// and the rest. Look up if we already have 'the stuff factored out'.
	// If not, create new. If so, add this term.

	Ex_comparator comparator(kernel.properties);

	typedef std::pair<Ex, std::vector<Ex> > new_term_t;
	std::vector<new_term_t> new_terms;

	auto term=tr.begin(it);
	while(term!=tr.end(it)) {
		auto next_term=term;
		++next_term;

		iterator prod=term;
		prod_wrap_single_term(prod);

		Ex collector("\\prod"); // collect all factors that we have taken out

		std::cerr << "Considering " << Ex(prod) << std::endl;

		// Insert a dummy symbol at the very front.
		auto dummy = tr.prepend_child(prod, str_node("dummy"));

		// Look at all factors in turn and determine if they should be taken out.
		auto fac=tr.begin(prod);
		while(fac!=tr.end(prod)) {
			auto next=fac;
			++next;
			for(size_t i=0; i<to_factor_out.size(); ++i) {
				auto match=comparator.equal_subtree(fac, to_factor_out[i].begin());
				if(match==Ex_comparator::subtree_match) {
					int sign=comparator.can_move_adjacent(prod, dummy, fac);
					if(sign!=0) {
						collector.append_child(collector.begin(), iterator(fac));
						next=tr.erase(fac);
						result=result_t::l_applied;
						break;
						}
					}
				}
			fac=next;
			}
		tr.erase(dummy);
		if(tr.number_of_children(prod)==0)
			tr.append_child(prod, str_node("1"));

		if(collector.number_of_children(collector.begin())!=0) { 
			// The stuff factored out of this term is in 'collector'. See if we have 
			// factored out that thing before.
			
			bool found=false;
			for(auto& nt: new_terms) {
				if(nt.first==collector) {
					nt.second.push_back(Ex(prod));
					found=true;
					break;
					}
				}
			if(!found) {
				std::vector<Ex> v;
				v.push_back(Ex(prod));
				new_term_t nt(collector, v);
				new_terms.push_back(nt);
				}

			// All info is now in new_terms; can remove the original.
			tr.erase(prod);
			}
		else {
			prod_unwrap_single_term(prod);
			}
		term=next_term;
		}

	std::cerr << "result " << Ex(it) << std::endl;

	// Everything has been collected now into new_terms. Expand those out
	// into a proper sum of products.

	for(auto& nt: new_terms) {
		auto prod = tr.append_child(it, nt.first.begin());
		if(nt.second.size()==1) { // factored, but only one term found.
			tr.append_children(prod, nt.second[0].begin(), nt.second[0].end());
			}
		else {
			auto sum = tr.append_child(prod, str_node("\\sum"));
			for(auto& term: nt.second) { 
				auto tmp = tr.append_child(sum, term.begin());
				cleanup_dispatch(kernel, tr, tmp);
				}
			}
		}
	
	return result;
	}
