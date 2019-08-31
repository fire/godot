// Copyright 2010-2018 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ortools/flatzinc/model.h"

#include <set>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/flatzinc/logging.h"

namespace operations_research {
namespace fz {
// ----- Domain -----

Domain Domain::IntegerList(std::vector<int64> values) {
  Domain result;
  result.is_interval = false;
  result.values = std::move(values);
  gtl::STLSortAndRemoveDuplicates(&result.values);
  result.display_as_boolean = false;
  result.is_a_set = false;
  return result;
}

Domain Domain::AllInt64() {
  Domain result;
  result.is_interval = true;
  result.display_as_boolean = false;
  result.is_a_set = false;
  return result;
}

Domain Domain::IntegerValue(int64 value) {
  Domain result;
  result.is_interval = false;
  result.values.push_back(value);
  result.display_as_boolean = false;
  result.is_a_set = false;
  return result;
}

Domain Domain::Interval(int64 included_min, int64 included_max) {
  Domain result;
  result.is_interval = true;
  result.display_as_boolean = false;
  result.values.push_back(included_min);
  result.values.push_back(included_max);
  result.is_a_set = false;
  return result;
}

Domain Domain::Boolean() {
  Domain result;
  result.is_interval = false;
  result.display_as_boolean = true;
  result.values.push_back(0);
  result.values.push_back(1);
  result.is_a_set = false;
  return result;
}

Domain Domain::SetOfIntegerList(std::vector<int64> values) {
  Domain result = IntegerList(std::move(values));
  result.is_a_set = true;
  return result;
}

Domain Domain::SetOfAllInt64() {
  Domain result = AllInt64();
  result.is_a_set = true;
  return result;
}

Domain Domain::SetOfIntegerValue(int64 value) {
  Domain result = IntegerValue(value);
  result.is_a_set = true;
  return result;
}

Domain Domain::SetOfInterval(int64 included_min, int64 included_max) {
  Domain result = Interval(included_min, included_max);
  result.is_a_set = true;
  return result;
}

Domain Domain::SetOfBoolean() {
  Domain result = Boolean();
  result.is_a_set = true;
  return result;
}

Domain Domain::EmptyDomain() {
  Domain result;
  result.is_interval = false;
  result.display_as_boolean = false;
  result.is_a_set = false;
  return result;
}

bool Domain::IntersectWithDomain(const Domain& domain) {
  if (domain.is_interval) {
    if (!domain.values.empty()) {
      return IntersectWithInterval(domain.values[0], domain.values[1]);
    }
    return false;
  }
  if (is_interval) {
    is_interval = false;  // Other is not an interval.
    if (values.empty()) {
      values = domain.values;
    } else {
      const int64 imin = values[0];
      const int64 imax = values[1];
      values = domain.values;
      IntersectWithInterval(imin, imax);
    }
    return true;
  }
  // now deal with the intersection of two lists of values
  return IntersectWithListOfIntegers(domain.values);
}

bool Domain::IntersectWithSingleton(int64 value) {
  return IntersectWithInterval(value, value);
}

bool Domain::IntersectWithInterval(int64 interval_min, int64 interval_max) {
  if (interval_min > interval_max) {  // Empty interval -> empty domain.
    is_interval = false;
    values.clear();
    return true;
  } else if (is_interval) {
    if (values.empty()) {
      values.push_back(interval_min);
      values.push_back(interval_max);
      return true;
    } else {
      if (values[0] >= interval_min && values[1] <= interval_max) return false;
      values[0] = std::max(values[0], interval_min);
      values[1] = std::min(values[1], interval_max);
      if (values[0] > values[1]) {
        values.clear();
        is_interval = false;
      } else if (values[0] == values[1]) {
        is_interval = false;
        values.pop_back();
      }
      return true;
    }
  } else {
    if (!values.empty()) {
      std::sort(values.begin(), values.end());
      std::vector<int64> new_values;
      new_values.reserve(values.size());
      bool changed = false;
      for (const int64 val : values) {
        if (val > interval_max) {
          changed = true;
          break;
        }
        if (val >= interval_min &&
            (new_values.empty() || val != new_values.back())) {
          new_values.push_back(val);
        } else {
          changed = true;
        }
      }
      values.swap(new_values);
      return changed;
    }
  }
  return false;
}

bool Domain::IntersectWithListOfIntegers(const std::vector<int64>& integers) {
  if (is_interval) {
    const int64 dmin = values.empty() ? kint64min : values[0];
    const int64 dmax = values.empty() ? kint64max : values[1];
    values.clear();
    for (const int64 v : integers) {
      if (v >= dmin && v <= dmax) values.push_back(v);
    }
    gtl::STLSortAndRemoveDuplicates(&values);
    if (!values.empty() &&
        values.back() - values.front() == values.size() - 1 &&
        values.size() >= 2) {
      if (values.size() > 2) {
        // Contiguous case.
        const int64 last = values.back();
        values.resize(2);
        values[1] = last;
      }
      return values[0] != dmin || values[1] != dmax;
    } else {
      // This also covers and invalid (empty) domain.
      is_interval = false;
      return true;
    }
  } else {
    // TODO(user): Investigate faster code for small arrays.
    std::sort(values.begin(), values.end());
    absl::flat_hash_set<int64> other_values(integers.begin(), integers.end());
    std::vector<int64> new_values;
    new_values.reserve(std::min(values.size(), integers.size()));
    bool changed = false;
    for (const int64 val : values) {
      if (gtl::ContainsKey(other_values, val)) {
        if (new_values.empty() || val != new_values.back()) {
          new_values.push_back(val);
        }
      } else {
        changed = true;
      }
    }
    values.swap(new_values);
    return changed;
  }
}

bool Domain::HasOneValue() const {
  return (values.size() == 1 || (values.size() == 2 && values[0] == values[1]));
}

bool Domain::empty() const {
  return is_interval ? (values.size() == 2 && values[0] > values[1])
                     : values.empty();
}

int64 Domain::Min() const {
  CHECK(!empty());
  return is_interval && values.empty() ? kint64min : values.front();
}

int64 Domain::Max() const {
  CHECK(!empty());
  return is_interval && values.empty() ? kint64max : values.back();
}

int64 Domain::Value() const {
  CHECK(HasOneValue());
  return values.front();
}

bool Domain::IsAllInt64() const {
  return is_interval &&
         (values.empty() || (values[0] == kint64min && values[1] == kint64max));
}

bool Domain::Contains(int64 value) const {
  if (is_interval) {
    if (values.empty()) {
      return true;
    } else {
      return value >= values[0] && value <= values[1];
    }
  } else {
    return std::find(values.begin(), values.end(), value) != values.end();
  }
}

namespace {
bool IntervalOverlapValues(int64 lb, int64 ub,
                           const std::vector<int64>& values) {
  for (int64 value : values) {
    if (lb <= value && value <= ub) {
      return true;
    }
  }
  return false;
}
}  // namespace

bool Domain::OverlapsIntList(const std::vector<int64>& vec) const {
  if (IsAllInt64()) {
    return true;
  }
  if (is_interval) {
    CHECK(!values.empty());
    return IntervalOverlapValues(values[0], values[1], vec);
  } else {
    // TODO(user): Better algorithm, sort and compare increasingly.
    const std::vector<int64>& to_scan =
        values.size() <= vec.size() ? values : vec;
    const absl::flat_hash_set<int64> container =
        values.size() <= vec.size()
            ? absl::flat_hash_set<int64>(vec.begin(), vec.end())
            : absl::flat_hash_set<int64>(values.begin(), values.end());
    for (int64 value : to_scan) {
      if (gtl::ContainsKey(container, value)) {
        return true;
      }
    }
    return false;
  }
}

bool Domain::OverlapsIntInterval(int64 lb, int64 ub) const {
  if (IsAllInt64()) {
    return true;
  }
  if (is_interval) {
    CHECK(!values.empty());
    const int64 dlb = values[0];
    const int64 dub = values[1];
    return !(dub < lb || dlb > ub);
  } else {
    return IntervalOverlapValues(lb, ub, values);
  }
}

bool Domain::OverlapsDomain(const Domain& other) const {
  if (other.is_interval) {
    if (other.values.empty()) {
      return true;
    } else {
      return OverlapsIntInterval(other.values[0], other.values[1]);
    }
  } else {
    return OverlapsIntList(other.values);
  }
}

bool Domain::RemoveValue(int64 value) {
  if (is_interval) {
    if (values.empty()) {
      return false;
    } else if (value == values[0] && value != values[1]) {
      values[0]++;
      return true;
    } else if (value == values[1] && value != values[0]) {
      values[1]--;
      return true;
    } else if (values[1] - values[0] < 1024 && value > values[0] &&
               value < values[1]) {  // small
      const int64 vmax = values[1];
      values.pop_back();
      values.reserve(vmax - values[0]);
      for (int64 v = values[0] + 1; v <= vmax; ++v) {
        if (v != value) {
          values.push_back(v);
        }
      }
      is_interval = false;
      return true;
    }
  } else {
    values.erase(std::remove(values.begin(), values.end(), value),
                 values.end());
    return true;
  }
  return false;
}

std::string Domain::DebugString() const {
  if (is_interval) {
    if (values.empty()) {
      return "int";
    } else {
      return absl::StrFormat("[%d..%d]", values[0], values[1]);
    }
  } else if (values.size() == 1) {
    return absl::StrCat(values.back());
  } else {
    return absl::StrFormat("[%s]", absl::StrJoin(values, ", "));
  }
}

// ----- Argument -----

Argument Argument::IntegerValue(int64 value) {
  Argument result;
  result.type = INT_VALUE;
  result.values.push_back(value);
  return result;
}

Argument Argument::Interval(int64 imin, int64 imax) {
  Argument result;
  result.type = INT_INTERVAL;
  result.values.push_back(imin);
  result.values.push_back(imax);
  return result;
}

Argument Argument::IntegerList(std::vector<int64> values) {
  Argument result;
  result.type = INT_LIST;
  result.values = std::move(values);
  return result;
}

Argument Argument::DomainList(std::vector<Domain> domains) {
  Argument result;
  result.type = DOMAIN_LIST;
  result.domains = std::move(domains);
  return result;
}

Argument Argument::IntVarRef(IntegerVariable* const var) {
  Argument result;
  result.type = INT_VAR_REF;
  result.variables.push_back(var);
  return result;
}

Argument Argument::IntVarRefArray(std::vector<IntegerVariable*> vars) {
  Argument result;
  result.type = INT_VAR_REF_ARRAY;
  result.variables = std::move(vars);
  return result;
}

Argument Argument::VoidArgument() {
  Argument result;
  result.type = VOID_ARGUMENT;
  return result;
}

Argument Argument::FromDomain(const Domain& domain) {
  if (domain.is_interval) {
    if (domain.values.empty()) {
      return Argument::Interval(kint64min, kint64max);
    } else {
      return Argument::Interval(domain.values[0], domain.values[1]);
    }
  } else {
    return Argument::IntegerList(domain.values);
  }
}

std::string Argument::DebugString() const {
  switch (type) {
    case INT_VALUE:
      return absl::StrFormat("% d", values[0]);
    case INT_INTERVAL:
      return absl::StrFormat("[%d..%d]", values[0], values[1]);
    case INT_LIST:
      return absl::StrFormat("[%s]", absl::StrJoin(values, ", "));
    case DOMAIN_LIST:
      return absl::StrFormat("[%s]", JoinDebugString(domains, ", "));
    case INT_VAR_REF:
      return variables[0]->name;
    case INT_VAR_REF_ARRAY: {
      std::string result = "[";
      for (int i = 0; i < variables.size(); ++i) {
        result.append(variables[i]->name);
        result.append(i != variables.size() - 1 ? ", " : "]");
      }
      return result;
    }
    case VOID_ARGUMENT:
      return "VoidArgument";
  }
  LOG(FATAL) << "Unhandled case in DebugString " << static_cast<int>(type);
  return "";
}

bool Argument::IsVariable() const { return type == INT_VAR_REF; }

bool Argument::HasOneValue() const {
  return (type == INT_VALUE || (type == INT_LIST && values.size() == 1) ||
          (type == INT_INTERVAL && values[0] == values[1]) ||
          (type == INT_VAR_REF && variables[0]->domain.HasOneValue()));
}

int64 Argument::Value() const {
  DCHECK(HasOneValue()) << "Value() called on unbound Argument: "
                        << DebugString();
  switch (type) {
    case INT_VALUE:
    case INT_INTERVAL:
    case INT_LIST:
      return values[0];
    case INT_VAR_REF: {
      return variables[0]->domain.values[0];
    }
    default: {
      LOG(FATAL) << "Should not be here";
      return 0;
    }
  }
}

bool Argument::IsArrayOfValues() const {
  switch (type) {
    case INT_VALUE:
      return false;
    case INT_INTERVAL:
      return false;
    case INT_LIST:
      return true;
    case DOMAIN_LIST: {
      for (const Domain& domain : domains) {
        if (!domain.HasOneValue()) {
          return false;
        }
      }
      return true;
    }
    case INT_VAR_REF:
      return false;
    case INT_VAR_REF_ARRAY: {
      for (IntegerVariable* var : variables) {
        if (!var->domain.HasOneValue()) {
          return false;
        }
      }
      return true;
    }
    case VOID_ARGUMENT:
      return false;
  }
}

bool Argument::Contains(int64 value) const {
  switch (type) {
    case Argument::INT_LIST: {
      return std::find(values.begin(), values.end(), value) != values.end();
    }
    case Argument::INT_INTERVAL: {
      return value >= values.front() && value <= values.back();
    }
    case Argument::INT_VALUE: {
      return value == values.front();
    }
    default: {
      LOG(FATAL) << "Cannot call Contains() on " << DebugString();
      return false;
    }
  }
}

int64 Argument::ValueAt(int pos) const {
  switch (type) {
    case INT_LIST:
      CHECK_GE(pos, 0);
      CHECK_LT(pos, values.size());
      return values[pos];
    case DOMAIN_LIST: {
      CHECK_GE(pos, 0);
      CHECK_LT(pos, domains.size());
      CHECK(domains[pos].HasOneValue());
      return domains[pos].Value();
    }
    case INT_VAR_REF_ARRAY: {
      CHECK_GE(pos, 0);
      CHECK_LT(pos, variables.size());
      CHECK(variables[pos]->domain.HasOneValue());
      return variables[pos]->domain.Value();
    }
    default: {
      LOG(FATAL) << "Should not be here";
      return 0;
    }
  }
}

IntegerVariable* Argument::Var() const {
  return type == INT_VAR_REF ? variables[0] : nullptr;
}

IntegerVariable* Argument::VarAt(int pos) const {
  return type == INT_VAR_REF_ARRAY ? variables[pos] : nullptr;
}

// ----- IntegerVariable -----

IntegerVariable::IntegerVariable(const std::string& name_,
                                 const Domain& domain_, bool temporary_)
    : name(name_),
      domain(domain_),
      defining_constraint(nullptr),
      temporary(temporary_),
      active(true) {
  if (!domain.is_interval) {
    gtl::STLSortAndRemoveDuplicates(&domain.values);
  }
}

bool IntegerVariable::Merge(const std::string& other_name,
                            const Domain& other_domain,
                            Constraint* const other_constraint,
                            bool other_temporary) {
  if (defining_constraint != nullptr && other_constraint != nullptr) {
    // Both are defined, we cannot merge the two variables.
    return false;
  }
  if (temporary && !other_temporary) {
    temporary = false;
    name = other_name;
  }
  if (defining_constraint == nullptr) {
    defining_constraint = other_constraint;
    if (defining_constraint != nullptr) {
      defining_constraint->target_variable = this;
    }
  }
  domain.IntersectWithDomain(other_domain);
  return true;
}

std::string IntegerVariable::DebugString() const {
  if (!domain.is_interval && domain.values.size() == 1) {
    return absl::StrFormat("% d", domain.values.back());
  } else {
    return absl::StrFormat(
        "%s(%s%s%s)%s", name, domain.DebugString(),
        temporary ? ", temporary" : "",
        defining_constraint != nullptr ? ", target_variable" : "",
        active ? "" : " [removed during presolve]");
  }
}

// ----- Constraint -----

std::string Constraint::DebugString() const {
  const std::string strong = strong_propagation ? "strong propagation" : "";
  const std::string presolve_status_str =
      active ? ""
             : (presolve_propagation_done ? "[propagated during presolve]"
                                          : "[removed during presolve]");
  const std::string target =
      target_variable != nullptr
          ? absl::StrFormat(" => %s", target_variable->name)
          : "";
  return absl::StrFormat("%s(%s)%s %s %s", type,
                         JoinDebugString(arguments, ", "), target, strong,
                         presolve_status_str);
}

void Constraint::RemoveArg(int arg_pos) {
  arguments.erase(arguments.begin() + arg_pos);
}

void Constraint::MarkAsInactive() {
  RemoveTargetVariable();
  active = false;
  // TODO(user): Reclaim arguments and memory.
}

void Constraint::SetAsFalse() {
  RemoveTargetVariable();
  type = "false_constraint";
  arguments.clear();
}

void Constraint::RemoveTargetVariable() {
  if (target_variable != nullptr) {
    if (target_variable->defining_constraint == this) {
      FZVLOG << "  - remove target_variable from " << DebugString() << FZENDL;
      target_variable->defining_constraint = nullptr;
      target_variable = nullptr;
    } else {
      FZVLOG << "  - asymmetric relation " << DebugString() << FZENDL;
      target_variable = nullptr;
    }
  }
}

// ----- Annotation -----

Annotation Annotation::Empty() {
  Annotation result;
  result.type = ANNOTATION_LIST;
  result.interval_min = 0;
  result.interval_max = 0;
  return result;
}

Annotation Annotation::AnnotationList(std::vector<Annotation> list) {
  Annotation result;
  result.type = ANNOTATION_LIST;
  result.interval_min = 0;
  result.interval_max = 0;
  result.annotations = std::move(list);
  return result;
}

Annotation Annotation::Identifier(const std::string& id) {
  Annotation result;
  result.type = IDENTIFIER;
  result.interval_min = 0;
  result.interval_max = 0;
  result.id = id;
  return result;
}

Annotation Annotation::FunctionCallWithArguments(const std::string& id,
                                                 std::vector<Annotation> args) {
  Annotation result;
  result.type = FUNCTION_CALL;
  result.interval_min = 0;
  result.interval_max = 0;
  result.id = id;
  result.annotations = std::move(args);
  return result;
}

Annotation Annotation::FunctionCall(const std::string& id) {
  Annotation result;
  result.type = FUNCTION_CALL;
  result.interval_min = 0;
  result.interval_max = 0;
  result.id = id;
  return result;
}

Annotation Annotation::Interval(int64 interval_min, int64 interval_max) {
  Annotation result;
  result.type = INTERVAL;
  result.interval_min = interval_min;
  result.interval_max = interval_max;
  return result;
}

Annotation Annotation::IntegerValue(int64 value) {
  Annotation result;
  result.type = INT_VALUE;
  result.interval_min = value;
  return result;
}

Annotation Annotation::Variable(IntegerVariable* const var) {
  Annotation result;
  result.type = INT_VAR_REF;
  result.interval_min = 0;
  result.interval_max = 0;
  result.variables.push_back(var);
  return result;
}

Annotation Annotation::VariableList(std::vector<IntegerVariable*> variables) {
  Annotation result;
  result.type = INT_VAR_REF_ARRAY;
  result.interval_min = 0;
  result.interval_max = 0;
  result.variables = std::move(variables);
  return result;
}

Annotation Annotation::String(const std::string& str) {
  Annotation result;
  result.type = STRING_VALUE;
  result.interval_min = 0;
  result.interval_max = 0;
  result.string_value = str;
  return result;
}

void Annotation::AppendAllIntegerVariables(
    std::vector<IntegerVariable*>* const vars) const {
  for (const Annotation& ann : annotations) {
    ann.AppendAllIntegerVariables(vars);
  }
  if (!variables.empty()) {
    vars->insert(vars->end(), variables.begin(), variables.end());
  }
}

std::string Annotation::DebugString() const {
  switch (type) {
    case ANNOTATION_LIST: {
      return absl::StrFormat("[%s]", JoinDebugString(annotations, ", "));
    }
    case IDENTIFIER: {
      return id;
    }
    case FUNCTION_CALL: {
      return absl::StrFormat("%s(%s)", id, JoinDebugString(annotations, ", "));
    }
    case INTERVAL: {
      return absl::StrFormat("%d..%d", interval_min, interval_max);
    }
    case INT_VALUE: {
      return absl::StrCat(interval_min);
    }
    case INT_VAR_REF: {
      return variables.front()->name;
    }
    case INT_VAR_REF_ARRAY: {
      std::string result = "[";
      for (int i = 0; i < variables.size(); ++i) {
        result.append(variables[i]->DebugString());
        result.append(i != variables.size() - 1 ? ", " : "]");
      }
      return result;
    }
    case STRING_VALUE: {
      return absl::StrFormat("\"%s\"", string_value);
    }
  }
  LOG(FATAL) << "Unhandled case in DebugString " << static_cast<int>(type);
  return "";
}

// ----- SolutionOutputSpecs -----

std::string SolutionOutputSpecs::Bounds::DebugString() const {
  return absl::StrFormat("%d..%d", min_value, max_value);
}

SolutionOutputSpecs SolutionOutputSpecs::SingleVariable(
    const std::string& name, IntegerVariable* variable,
    bool display_as_boolean) {
  SolutionOutputSpecs result;
  result.name = name;
  result.variable = variable;
  result.display_as_boolean = display_as_boolean;
  return result;
}

SolutionOutputSpecs SolutionOutputSpecs::MultiDimensionalArray(
    const std::string& name, std::vector<Bounds> bounds,
    std::vector<IntegerVariable*> flat_variables, bool display_as_boolean) {
  SolutionOutputSpecs result;
  result.variable = nullptr;
  result.name = name;
  result.bounds = std::move(bounds);
  result.flat_variables = std::move(flat_variables);
  result.display_as_boolean = display_as_boolean;
  return result;
}

SolutionOutputSpecs SolutionOutputSpecs::VoidOutput() {
  SolutionOutputSpecs result;
  result.variable = nullptr;
  result.display_as_boolean = false;
  return result;
}

std::string SolutionOutputSpecs::DebugString() const {
  if (variable != nullptr) {
    return absl::StrFormat("output_var(%s)", variable->name);
  } else {
    return absl::StrFormat("output_array([%s] [%s])",
                           JoinDebugString(bounds, ", "),
                           JoinNameFieldPtr(flat_variables, ", "));
  }
}

// ----- Model -----

Model::~Model() {
  gtl::STLDeleteElements(&variables_);
  gtl::STLDeleteElements(&constraints_);
}

IntegerVariable* Model::AddVariable(const std::string& name,
                                    const Domain& domain, bool defined) {
  IntegerVariable* const var = new IntegerVariable(name, domain, defined);
  variables_.push_back(var);
  return var;
}

// TODO(user): Create only once constant per value.
IntegerVariable* Model::AddConstant(int64 value) {
  IntegerVariable* const var = new IntegerVariable(
      absl::StrCat(value), Domain::IntegerValue(value), true);
  variables_.push_back(var);
  return var;
}

void Model::AddConstraint(const std::string& id,
                          std::vector<Argument> arguments, bool is_domain,
                          IntegerVariable* defines) {
  Constraint* const constraint =
      new Constraint(id, std::move(arguments), is_domain, defines);
  constraints_.push_back(constraint);
  if (defines != nullptr) {
    defines->defining_constraint = constraint;
  }
}

void Model::AddConstraint(const std::string& id,
                          std::vector<Argument> arguments) {
  AddConstraint(id, std::move(arguments), false, nullptr);
}

void Model::AddOutput(SolutionOutputSpecs output) {
  output_.push_back(std::move(output));
}

void Model::Satisfy(std::vector<Annotation> search_annotations) {
  objective_ = nullptr;
  search_annotations_ = std::move(search_annotations);
}

void Model::Minimize(IntegerVariable* obj,
                     std::vector<Annotation> search_annotations) {
  objective_ = obj;
  maximize_ = false;
  search_annotations_ = std::move(search_annotations);
}

void Model::Maximize(IntegerVariable* obj,
                     std::vector<Annotation> search_annotations) {
  objective_ = obj;
  maximize_ = true;
  search_annotations_ = std::move(search_annotations);
}

std::string Model::DebugString() const {
  std::string output = absl::StrFormat("Model %s\nVariables\n", name_);
  for (int i = 0; i < variables_.size(); ++i) {
    absl::StrAppendFormat(&output, "  %s\n", variables_[i]->DebugString());
  }
  output.append("Constraints\n");
  for (int i = 0; i < constraints_.size(); ++i) {
    if (constraints_[i] != nullptr) {
      absl::StrAppendFormat(&output, "  %s\n", constraints_[i]->DebugString());
    }
  }
  if (objective_ != nullptr) {
    absl::StrAppendFormat(&output, "%s %s\n  %s\n",
                          maximize_ ? "Maximize" : "Minimize", objective_->name,
                          JoinDebugString(search_annotations_, ", "));
  } else {
    absl::StrAppendFormat(&output, "Satisfy\n  %s\n",
                          JoinDebugString(search_annotations_, ", "));
  }
  output.append("Output\n");
  for (int i = 0; i < output_.size(); ++i) {
    absl::StrAppendFormat(&output, "  %s\n", output_[i].DebugString());
  }

  return output;
}

bool Model::IsInconsistent() const {
  for (IntegerVariable* var : variables_) {
    if (var->domain.empty()) {
      return true;
    }
  }
  for (Constraint* ct : constraints_) {
    if (ct->type == "false_constraint") {
      return true;
    }
  }

  return false;
}

// ----- Model statistics -----

void ModelStatistics::PrintStatistics() const {
  FZLOG << "Model " << model_.name() << FZENDL;
  for (const auto& it : constraints_per_type_) {
    FZLOG << "  - " << it.first << ": " << it.second.size() << FZENDL;
  }
  if (model_.objective() == nullptr) {
    FZLOG << "  - Satisfaction problem" << FZENDL;
  } else {
    FZLOG << "  - " << (model_.maximize() ? "Maximization" : "Minimization")
          << " problem" << FZENDL;
  }
}

void ModelStatistics::BuildStatistics() {
  constraints_per_type_.clear();
  constraints_per_variables_.clear();
  for (Constraint* const ct : model_.constraints()) {
    if (ct != nullptr && ct->active) {
      constraints_per_type_[ct->type].push_back(ct);
      absl::flat_hash_set<const IntegerVariable*> marked;
      for (const Argument& arg : ct->arguments) {
        for (IntegerVariable* const var : arg.variables) {
          marked.insert(var);
        }
      }
      for (const IntegerVariable* const var : marked) {
        constraints_per_variables_[var].push_back(ct);
      }
    }
  }
}

// Flatten Search annotations.
void FlattenAnnotations(const Annotation& ann, std::vector<Annotation>* out) {
  if (ann.type == Annotation::ANNOTATION_LIST ||
      ann.IsFunctionCallWithIdentifier("seq_search")) {
    for (const Annotation& inner : ann.annotations) {
      FlattenAnnotations(inner, out);
    }
  } else {
    out->push_back(ann);
  }
}

}  // namespace fz
}  // namespace operations_research
