
#include "algorithms/young_project.hh"

young_project::young_project(Kernel& k, exptree& tr)
	: Algorithm(k,tr), remove_traces(false)
	{
	if(it==tr.end()) return; // for in-program calls

	if(number_of_args()==2) {
		sibling_iterator arg=args_begin();
		if(*arg->name=="\\comma") {
			++arg;
			if(*arg->name=="\\comma") {
				sibling_iterator arg=args_begin();
				sibling_iterator ycb=tr.begin(arg), yce=tr.end(arg);
				++arg;
				sibling_iterator icb=tr.begin(arg), ice=tr.end(arg);
				unsigned int rownum=0;
				while(ycb!=yce) {
					for(unsigned int r=0; r<*ycb->multiplier; ++r) {
						if(icb==ice) 
							throw consistency_error("out of range");
						if(icb->is_rational()) { // index given by position
							if(*icb->multiplier<0)
								throw consistency_error("index labels out of range");
							tab.add_box(rownum,to_long(*icb->multiplier));
							}
						else { // index given by name; store in the other tableau
							nametab.add_box(rownum, icb);
							}
						++icb;
						}
					++rownum;
					++ycb;
					}
				return;
				}
			}
		}
	throw algorithm::constructor_error();
	}

bool young_project::can_apply(iterator it)
	{
	if(*it->name!="\\prod") {
		if(!is_single_term(it)) {
			return false;
			}
		}

	prod_wrap_single_term(it);
	if(nametab.number_of_rows()>0) { // have to convert names to numbers
		tab.copy_shape(nametab);
		pos_tab_t::iterator pi=tab.begin();
		name_tab_t::iterator ni=nametab.begin();
		while(ni!=nametab.end()) {
			exptree::index_iterator ii=tr.begin_index(it);
			unsigned int indexnum=0;
			while(ii!=tr.end_index(it)) {
				if(subtree_exact_equal(ii, *ni)) {
					*pi=indexnum;
					break;
					}
				++indexnum;
				++ii;
				}
			if(indexnum==tr.number_of_indices(it)) {
				prod_unwrap_single_term(it);
				return false; // cannot find indicated index in expression
				}
			++pi;
			++ni;
			}
		}

	prod_unwrap_single_term(it);
	return true;
	}

exptree::iterator young_project::nth_index_node(iterator it, unsigned int num)
	{
	exptree::fixed_depth_iterator indname=tr.begin_fixed(it, 2);
	indname+=num;
	return indname;
	}

Algorithm::result_t young_project::apply(iterator& it)
	{
	prod_wrap_single_term(it);
	sym.clear();
	
	if(asym_ranges.size()>0) {
		// Convert index locations to box numbers.
		combin::range_vector_t sublengths_scattered;
//		txtout << "asym_ranges: ";
		for(unsigned int i=0; i<asym_ranges.size(); ++i) {
			combin::range_t newr;
			for(unsigned int j=0; j<asym_ranges[i].size(); ++j) {
				// Search asym_ranges[i][j]
				int offs=0;
				pos_tab_t::iterator tt=tab.begin();
				while(tt!=tab.end()) {
					if((*tt)==asym_ranges[i][j]) {
						newr.push_back(offs);
//						txtout << asym_ranges[i][j] << " ";
						break;
						}
					++tt;
					++offs;
					}
				}
			sublengths_scattered.push_back(newr);
//			txtout << std::endl;
			} 
		tab.projector(sym, sublengths_scattered);
		}
	else tab.projector(sym);

	// FIXME: We can also compress the result by sorting all 
	// locations which belong to the same asym set. This could actually
	// be done in combinatorics already.

	exptree rep;
	rep.set_head(str_node("\\sum"));
	for(unsigned int i=0; i<sym.size(); ++i) {
		// Generate the term.
		exptree repfac(it);
		for(unsigned int j=0; j<sym[i].size(); ++j) {
			exptree::index_iterator src_fd=tr.begin_index(it);
			exptree::index_iterator dst_fd=tr.begin_index(repfac.begin());
			src_fd+=sym[i][j];        // take the index at location sym[i][j]
			dst_fd+=sym.original[j];  // and store it in location sym.original[j]
			tr.replace_index(dst_fd, src_fd); 
			}
		// Remove traces of antisymmetric objects. This can really
		// only be done here, since combinatorics.hh does not know
		// about index values, only about index locations. Note: we also
		// have to remove the entry in sym.original & sym.multiplicity if
		// we decide that a term vanishes.
		// IMPORTANT: if there are still permutations by value to be
		// done afterwards, do not use this!
		if(remove_traces) {
			for(unsigned int k=0; k<asym_ranges.size(); ++k) {
				for(unsigned int kk=0; kk<asym_ranges[k].size(); ++kk) {
					exptree::index_iterator it1=repfac.begin_index(repfac.begin());
					it1+=asym_ranges[k][kk];
					for(unsigned int kkk=kk+1; kkk<asym_ranges[k].size(); ++kkk) {
						exptree::index_iterator it2=repfac.begin_index(repfac.begin());
						it2+=asym_ranges[k][kkk];
						if(subtree_exact_equal(it1,it2)) {
							sym.set_multiplicity(i,0);
							goto traceterm;
							}
						}
					}
				} 
			}

		{ multiply(repfac.begin()->multiplier, sym.signature(i));
		multiply(repfac.begin()->multiplier, tab.projector_normalisation());
		iterator repfactop=repfac.begin();
		prod_unwrap_single_term(repfactop);
		rep.append_child(rep.begin(), repfac.begin()); }

	   traceterm: ;
		}
	it=tr.replace(it,rep.begin());
	expression_modified=true;

	sym.remove_multiplicity_zero();

	return l_applied;
	}
