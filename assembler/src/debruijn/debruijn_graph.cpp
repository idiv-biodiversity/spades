#include "debruijn_graph.hpp"
#include "logging.hpp"
#include "visualization_utils.hpp"

namespace debruijn_graph {

Sequence DeBruijnGraph::VertexNucls(VertexId v) const {
	if (v->outgoing_edges_.size() > 0) {
		return v->outgoing_edges_[0]->nucls().Subseq(0, k_);
	} else if (v->conjugate_->outgoing_edges_.size() > 0) {
		return !VertexNucls(v->conjugate_);
	}
	assert(false);
}

EdgeId DeBruijnGraph::AddSingleEdge(VertexId v1, VertexId v2,
		const Sequence& s, size_t coverage) {
	EdgeId newEdge = new Edge(s, v2, coverage);
	v1->AddOutgoingEdge(newEdge);
	return newEdge;
}

void DeBruijnGraph::DeleteAllOutgoing(Vertex *v) {
	vector<EdgeId> out = v->outgoing_edges_;
	for (vector<EdgeId>::iterator it = out.begin(); it != out.end(); ++it) {
		DeleteEdge(*it);
	}
}

const vector<EdgeId> DeBruijnGraph::OutgoingEdges(VertexId v) const {
	return v->outgoing_edges_;
}

const vector<EdgeId> DeBruijnGraph::IncomingEdges(VertexId v) const {
	vector<EdgeId> result;
	VertexId rcv = conjugate(v);
	vector<EdgeId> edges = rcv->OutgoingEdges();
	for (EdgeIterator it = edges.begin(); it != edges.end(); ++it) {
		result.push_back(conjugate(*it));
	}
	return result;
}

const vector<EdgeId> DeBruijnGraph::IncidentEdges(VertexId v) const {
	vector<EdgeId> result;
	DEBUG("Incident for vert: "<< v);
	for (EdgeIterator it = v->begin(); it != v->end(); ++it) {
		DEBUG("out:"<< *it);
		result.push_back(*it);
	}
	VertexId rcv = conjugate(v);

	for (EdgeIterator it = rcv->begin(); it != rcv->end(); ++it) {
		int fl = 1;
		for (int j = 0, sz = result.size(); j < sz; j++) {
			if (result[j] == *it) {
				fl = 0;
				break;
			}
		}
		if (fl) {
			DEBUG("in:"<< *it);
			result.push_back(conjugate(*it));
		}
	}
	return result;
}

const vector<EdgeId> DeBruijnGraph::NeighbouringEdges(EdgeId e) const {
	VertexId v_out = EdgeEnd(e);
	VertexId v_in = EdgeStart(e);
	vector<EdgeId> result = DeBruijnGraph::IncidentEdges(v_in);
	vector<EdgeId> out_res = DeBruijnGraph::IncidentEdges(v_out);
	// these vectors are small, and linear time is less than log in this case.
	for (vector<EdgeId>::iterator it = out_res.begin(); it != out_res.end(); ++it) {
		int fl = 1;
		for (int j = 0, sz = result.size(); j < sz; j++)
			if (result[j] == *it) {
				fl = 0;
				break;
			}

		if (fl)
			result.push_back(*it);
	}
	DEBUG(result.size());
	return result;
}

void DeBruijnGraph::FireAddVertex(VertexId v) {
	for (vector<ActionHandler*>::iterator it = action_handler_list_.begin(); it
			!= action_handler_list_.end(); ++it) {
		applier_.ApplyAdd(*it, v);
	}
}

void DeBruijnGraph::FireAddEdge(EdgeId edge) {
	for (vector<ActionHandler*>::iterator it = action_handler_list_.begin(); it
			!= action_handler_list_.end(); ++it) {
		applier_.ApplyAdd(*it, edge);
	}
}

void DeBruijnGraph::FireDeleteVertex(VertexId v) {
	for (vector<ActionHandler*>::iterator it = action_handler_list_.begin(); it
			!= action_handler_list_.end(); ++it) {
		applier_.ApplyDelete(*it, v);
	}
}

void DeBruijnGraph::FireDeleteEdge(EdgeId edge) {
	for (vector<ActionHandler*>::iterator it = action_handler_list_.begin(); it
			!= action_handler_list_.end(); ++it) {
		applier_.ApplyDelete(*it, edge);
	}
}

void DeBruijnGraph::FireMerge(vector<EdgeId> oldEdges, EdgeId newEdge) {
	for (vector<ActionHandler*>::iterator it = action_handler_list_.begin(); it
			!= action_handler_list_.end(); ++it) {
		applier_.ApplyMerge(*it, oldEdges, newEdge);
	}
}

void DeBruijnGraph::FireGlue(EdgeId new_edge, EdgeId edge1, EdgeId edge2) {
	for (vector<ActionHandler*>::iterator it = action_handler_list_.begin(); it
			!= action_handler_list_.end(); ++it) {
		applier_.ApplyGlue(*it, new_edge, edge1, edge2);
	}
}

void DeBruijnGraph::FireSplit(EdgeId edge, EdgeId newEdge1, EdgeId newEdge2) {
	for (vector<ActionHandler*>::iterator it = action_handler_list_.begin(); it
			!= action_handler_list_.end(); ++it) {
		applier_.ApplySplit(*it, edge, newEdge1, newEdge2);
	}
}

VertexId DeBruijnGraph::HiddenAddVertex() {
	VertexId v1 = new Vertex();
	VertexId v2 = new Vertex();
	v1->Setconjugate(v2);
	v2->Setconjugate(v1);
	vertices_.insert(v1);
	vertices_.insert(v2);
	return v1;
}

VertexId DeBruijnGraph::AddVertex() {
	VertexId result = HiddenAddVertex();
	FireAddVertex(result);
	return result;
}

void DeBruijnGraph::DeleteVertex(VertexId v) {
	assert(IsDeadEnd(v) && IsDeadStart(v));
	assert(v != NULL);
	FireDeleteVertex(v);
	VertexId conjugate = v->conjugate();
	vertices_.erase(v);
	delete v;
	vertices_.erase(conjugate);
	delete conjugate;
}

void DeBruijnGraph::ForceDeleteVertex(VertexId v) {
	DeleteAllOutgoing(v);
	DeleteAllOutgoing(v->conjugate());
	DeleteVertex(v);
}

EdgeId DeBruijnGraph::HiddenAddEdge(VertexId v1, VertexId v2,
		const Sequence &nucls, size_t coverage) {
	assert(vertices_.find(v1) != vertices_.end() && vertices_.find(v2) != vertices_.end());
	assert(nucls.size() >= k_ + 1);
	//	assert(OutgoingEdge(v1, nucls[k_]) == NULL);
	EdgeId result = AddSingleEdge(v1, v2, nucls, coverage);
	EdgeId rcEdge = result;
	if (nucls != !nucls) {
		rcEdge = AddSingleEdge(v2->conjugate(), v1->conjugate(), !nucls,
				coverage);
	}
	result->set_conjugate(rcEdge);
	rcEdge->set_conjugate(result);
	return result;
}

EdgeId DeBruijnGraph::AddEdge(VertexId v1, VertexId v2, const Sequence &nucls,
		size_t coverage) {
	EdgeId result = HiddenAddEdge(v1, v2, nucls, coverage);
	FireAddEdge(result);
	return result;
}

void DeBruijnGraph::DeleteEdge(EdgeId edge) {
	FireDeleteEdge(edge);
	EdgeId rcEdge = conjugate(edge);
	VertexId rcStart = conjugate(edge->end());
	VertexId start = conjugate(rcEdge->end());
	start->RemoveOutgoingEdge(edge);
	rcStart->RemoveOutgoingEdge(rcEdge);
	if (edge != rcEdge) {
		delete rcEdge;
	}
	delete edge;
}

bool DeBruijnGraph::AreLinkable(VertexId v1, VertexId v2, const Sequence &nucls) const {
	return VertexNucls(v1) == nucls.Subseq(0, k_) && VertexNucls(
			v2->conjugate()) == (!nucls).Subseq(0, k_);
}

EdgeId DeBruijnGraph::OutgoingEdge(VertexId v, char nucl) const {
	vector<EdgeId> edges = v->OutgoingEdges();
	for (EdgeIterator iter = edges.begin(); iter != edges.end(); ++iter) {
		char lastNucl = (*iter)->nucls()[k_];
		if (lastNucl == nucl) {
			return *iter;
		}
	}
	return NULL;
}

VertexId DeBruijnGraph::conjugate(VertexId v) const {
	return v->conjugate();
}

EdgeId DeBruijnGraph::conjugate(EdgeId edge) const {
	return edge->conjugate();
}

VertexId DeBruijnGraph::EdgeStart(EdgeId edge) const {
	return conjugate(edge)->end()->conjugate();
}

VertexId DeBruijnGraph::EdgeEnd(EdgeId edge) const {
	return edge->end();
}

bool DeBruijnGraph::CanCompressVertex(VertexId v) const {
	return v->OutgoingEdgeCount() == 1 && v->conjugate()->OutgoingEdgeCount()
			== 1;
}

void DeBruijnGraph::CompressVertex(VertexId v) {
	//assert(CanCompressVertex(v));
	if (CanCompressVertex(v)) {
		Merge(GetUniqueIncomingEdge(v), GetUniqueOutgoingEdge(v));
	}
}

void DeBruijnGraph::Merge(EdgeId edge1, EdgeId edge2) {
	assert(EdgeEnd(edge1) == EdgeStart(edge2));
	vector<EdgeId> toCompress;
	toCompress.push_back(edge1);
	toCompress.push_back(edge2);
	MergePath(toCompress);
}

vector<EdgeId> DeBruijnGraph::CorrectMergePath(const vector<EdgeId>& path) {
	vector<EdgeId> result;
	for (size_t i = 0; i < path.size(); i++) {
		if (path[i] == conjugate(path[i])) {
			if (i < path.size() - 1 - i) {
				for (size_t j = 0; j < path.size(); j++)
					result.push_back(conjugate(path[path.size() - 1 - j]));
				i = path.size() - 1 - i;
			} else {
				result = path;
			}
			size_t size = 2 * i + 1;
			for (size_t j = result.size(); j < size; j++) {
				result.push_back(conjugate(result[size - 1 - j]));
			}
			return result;
		}
	}
	return path;
}

EdgeId DeBruijnGraph::MergePath(const vector<EdgeId>& path) {
	vector<EdgeId> correctedPath = CorrectMergePath(path);
	assert(!correctedPath.empty());
	EdgeId newEdge = AddMergedEdge(correctedPath);
	FireMerge(correctedPath, newEdge);
	DeletePath(correctedPath);
	FireAddEdge(newEdge);
	return newEdge;
}

EdgeId DeBruijnGraph::AddMergedEdge(const vector<EdgeId> &path) {
	VertexId v1 = EdgeStart(path[0]);
	VertexId v2 = EdgeEnd(path[path.size() - 1]);
	SequenceBuilder sb;
	sb.append(EdgeNucls(path[0]).Subseq(0, k_));
	for (vector<EdgeId>::const_iterator it = path.begin(); it != path.end(); ++it) {
		sb.append(EdgeNucls(*it).Subseq(k_));
//		sb.append(EdgeNucls(*it));
	}
	return HiddenAddEdge(v1, v2, sb.BuildSequence());
}

void DeBruijnGraph::DeletePath(const vector<EdgeId> &path) {
	set<EdgeId> edgesToDelete;
	set<VertexId> verticesToDelete;
	edgesToDelete.insert(path[0]);
	for (size_t i = 0; i + 1 < path.size(); i++) {
		EdgeId e = path[i + 1];
		if (edgesToDelete.find(conjugate(e)) == edgesToDelete.end())
			edgesToDelete.insert(e);
		VertexId v = EdgeStart(e);
		if (verticesToDelete.find(conjugate(v)) == verticesToDelete.end())
			verticesToDelete.insert(v);
	}
	for (auto it = edgesToDelete.begin(); it != edgesToDelete.end(); ++it)
		DeleteEdge(*it);
	for (auto it = verticesToDelete.begin(); it != verticesToDelete.end(); ++it)
		DeleteVertex(*it);
}

pair<EdgeId, EdgeId> DeBruijnGraph::SplitEdge(EdgeId edge, size_t position) {
	assert(position >= 1 && position < length(edge));
	assert(edge != conjugate(edge));
	Sequence s1 = EdgeNucls(edge).Subseq(0, position + k_);
	Sequence s2 = EdgeNucls(edge).Subseq(position);
	assert(s1 + s2.Subseq(k_) == EdgeNucls(edge));
//	Sequence newSequence = s1 + s2.Subseq(k_);
	VertexId splitVertex = HiddenAddVertex();
	EdgeId newEdge1 = HiddenAddEdge(this->EdgeStart(edge), splitVertex, s1);
	EdgeId newEdge2 = HiddenAddEdge(splitVertex, this->EdgeEnd(edge), s2);
	FireSplit(edge, newEdge1, newEdge2);
	FireAddVertex(splitVertex);
	FireAddEdge(newEdge1);
	FireAddEdge(newEdge2);
	DeleteEdge(edge);
	return make_pair(newEdge1, newEdge2);
}

void DeBruijnGraph::GlueEdges(EdgeId edge1, EdgeId edge2) {
	EdgeId newEdge = HiddenAddEdge(EdgeStart(edge2), EdgeEnd(edge2), EdgeNucls(edge2));
	FireGlue(newEdge, edge1, edge2);
	FireDeleteEdge(edge1);
	FireDeleteEdge(edge2);
	FireAddEdge(newEdge);
	VertexId start = EdgeStart(edge1);
	VertexId end = EdgeEnd(edge1);
	DeleteEdge(edge1);
	if (IsDeadStart(start) && IsDeadEnd(start)) {
		DeleteVertex(start);
	}
	if (IsDeadStart(end) && IsDeadEnd(end)) {
		DeleteVertex(end);
	}
}

}
