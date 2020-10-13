#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/python/python.h>

/// Trampoline for derived types implemented in Python
MTS_VARIANT class PyPhaseFunction : public PhaseFunction<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(PhaseFunction, PhaseFunctionContext)

    PyPhaseFunction(const Properties &props) : PhaseFunction(props) {}

    std::pair<Vector3f, Float> sample(const PhaseFunctionContext &ctx,
                    const MediumInteraction3f &mi, const Point2f &sample,
                    Mask active) const override {
        using Return = std::pair<Vector3f, Float>;
        PYBIND11_OVERLOAD_PURE(Return, PhaseFunction, sample, ctx, mi, sample, active);
    }

    Float eval(const PhaseFunctionContext &ctx, const MediumInteraction3f &mi,
               const Vector3f &wo, Mask active) const override {
        PYBIND11_OVERLOAD_PURE(Float, PhaseFunction, eval, ctx, mi, wo, active);
    }

    Float projected_area(const MediumInteraction3f &mi, Mask active) const override {
        PYBIND11_OVERLOAD(Float, PhaseFunction, projected_area, mi, active);
    }

    Float max_projected_area() const override {
        PYBIND11_OVERLOAD(Float, PhaseFunction, max_projected_area, );
    }

    std::string to_string() const override {
        PYBIND11_OVERLOAD_PURE(std::string, PhaseFunction, to_string, );
    }
};

template <typename Ptr, typename Cls> void bind_phase_generic(Cls &cls) {
    MTS_PY_IMPORT_TYPES(PhaseFunctionContext)

    cls.def("sample",
            [](Ptr ptr, const PhaseFunctionContext &ctx,
               const MediumInteraction3f &mi, const Point2f &s,
               Mask active) { return ptr->sample(ctx, mi, s, active); },
            "ctx"_a, "mi"_a, "sample"_a, "active"_a = true,
            D(PhaseFunction, sample))
       .def("eval",
            [](Ptr ptr, const PhaseFunctionContext &ctx,
               const MediumInteraction3f &mi, const Vector3f &wo,
               Mask active) { return ptr->eval(ctx, mi, wo, active); },
            "ctx"_a, "mi"_a, "wo"_a, "active"_a = true,
            D(PhaseFunction, eval))
       .def("projected_area",
            [](Ptr ptr, const MediumInteraction3f &mi,
               Mask active) { return ptr->projected_area(mi, active); },
            "mi"_a, "active"_a = true,
            D(PhaseFunction, projected_area))
       .def("max_projected_area",
            [](Ptr ptr) { return ptr->max_projected_area(); },
            D(PhaseFunction, max_projected_area))
       .def("flags", [](Ptr ptr, Mask active) { return ptr->flags(active); },
            "active"_a = true, D(PhaseFunction, flags));

    if constexpr (ek::is_array_v<Ptr>)
        bind_enoki_ptr_array(cls);
}

MTS_PY_EXPORT(PhaseFunction) {
    MTS_PY_IMPORT_TYPES(PhaseFunction, PhaseFunctionContext, PhaseFunctionPtr)
    using PyPhaseFunction = PyPhaseFunction<Float, Spectrum>;

    m.def("has_flag", [](UInt32 flags, PhaseFunctionFlags f) { return has_flag(flags, f); });

    py::class_<PhaseFunctionContext>(m, "PhaseFunctionContext", D(PhaseFunctionContext))
        .def(py::init<Sampler*, TransportMode>(), "sampler"_a,
                "mode"_a = TransportMode::Radiance, D(PhaseFunctionContext, PhaseFunctionContext))
        .def_method(PhaseFunctionContext, reverse)
        .def_field(PhaseFunctionContext, sampler, D(PhaseFunctionContext, sampler))
        .def_repr(PhaseFunctionContext);

    auto phase =
        py::class_<PhaseFunction, PyPhaseFunction, Object, ref<PhaseFunction>>(
            m, "PhaseFunction", D(PhaseFunction))
            .def(py::init<const Properties &>())
            .def_method(PhaseFunction, id)
            .def("__repr__", &PhaseFunction::to_string);

    bind_phase_generic<PhaseFunction *>(phase);

    if constexpr (ek::is_array_v<PhaseFunctionPtr>) {
        py::object ek       = py::module_::import("enoki"),
                   ek_array = ek.attr("ArrayBase");

        py::class_<PhaseFunctionPtr> cls(m, "PhaseFunctionPtr", ek_array);
        bind_phase_generic<PhaseFunctionPtr>(cls);
        pybind11_type_alias<UInt32, ek::replace_scalar_t<UInt32, PhaseFunctionFlags>>();
    }

    MTS_PY_REGISTER_OBJECT("register_phasefunction", PhaseFunction)
}
