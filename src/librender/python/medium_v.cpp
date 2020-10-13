#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/python/python.h>

/// Trampoline for derived types implemented in Python
MTS_VARIANT class PyMedium : public Medium<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Medium, Sampler, Scene)

    PyMedium(const Properties &props) : Medium(props) {}

    std::tuple<Mask, Float, Float> intersect_aabb(const Ray3f &ray) const override {
        using Return = std::tuple<Mask, Float, Float>;
        PYBIND11_OVERLOAD_PURE(Return, Medium, intersect_aabb, ray);
    }

    UnpolarizedSpectrum get_combined_extinction(const MediumInteraction3f &mi, Mask active = true) const override {
        PYBIND11_OVERLOAD_PURE(UnpolarizedSpectrum, Medium, get_combined_extinction, mi, active);
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi, Mask active = true) const override {
        using Return = std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>;
        PYBIND11_OVERLOAD_PURE(Return, Medium, get_scattering_coefficients, mi, active);
    }

    std::string to_string() const override {
        PYBIND11_OVERLOAD_PURE(std::string, Medium, to_string, );
    }
};

template <typename Ptr, typename Cls> void bind_medium_generic(Cls &cls) {
    MTS_PY_IMPORT_TYPES(PhaseFunctionContext)

    cls.def("phase_function",
            [](Ptr ptr) { return ptr->phase_function(); },
            D(Medium, phase_function))
       .def("use_emitter_sampling",
            [](Ptr ptr) { return ptr->use_emitter_sampling(); },
            D(Medium, use_emitter_sampling))
       .def("is_homogeneous",
            [](Ptr ptr) { return ptr->is_homogeneous(); },
            D(Medium, is_homogeneous))
       .def("has_spectral_extinction",
            [](Ptr ptr) { return ptr->has_spectral_extinction(); },
            D(Medium, has_spectral_extinction))
       .def("get_combined_extinction",
            [](Ptr ptr, const MediumInteraction3f &mi, Mask active) {
                return ptr->get_combined_extinction(mi, active); },
            "mi"_a, "active"_a=true,
            D(Medium, get_combined_extinction))
       .def("intersect_aabb",
            [](Ptr ptr, const Ray3f &ray) {
                return ptr->intersect_aabb(ray); },
            "ray"_a,
            D(Medium, intersect_aabb))
       .def("sample_interaction",
            [](Ptr ptr, const Ray3f &ray, Float sample, UInt32 channel, Mask active) {
                return ptr->sample_interaction(ray, sample, channel, active); },
            "ray"_a, "sample"_a, "channel"_a, "active"_a,
            D(Medium, sample_interaction))
       .def("eval_tr_and_pdf",
            [](Ptr ptr, const MediumInteraction3f &mi,
               const SurfaceInteraction3f &si, Mask active) {
                return ptr->eval_tr_and_pdf(mi, si, active); },
            "mi"_a, "si"_a, "active"_a,
            D(Medium, eval_tr_and_pdf))
       .def("get_scattering_coefficients",
            [](Ptr ptr, const MediumInteraction3f &mi, Mask active = true) {
                return ptr->get_scattering_coefficients(mi, active); },
            "mi"_a, "active"_a=true,
            D(Medium, get_scattering_coefficients));

    if constexpr (ek::is_array_v<Ptr>)
        bind_enoki_ptr_array(cls);
}

MTS_PY_EXPORT(Medium) {
    MTS_PY_IMPORT_TYPES(Medium, MediumPtr, Scene, Sampler)
    using PyMedium = PyMedium<Float, Spectrum>;

    auto medium = py::class_<Medium, PyMedium, Object, ref<Medium>>(m, "Medium", D(Medium))
            .def(py::init<const Properties &>())
            .def_method(Medium, id)
            .def("__repr__", &Medium::to_string);

    bind_medium_generic<Medium *>(medium);

    if constexpr (ek::is_array_v<MediumPtr>) {
        py::object ek       = py::module_::import("enoki"),
                   ek_array = ek.attr("ArrayBase");

        py::class_<MediumPtr> cls(m, "MediumPtr", ek_array);
        bind_medium_generic<MediumPtr>(cls);
    }


    MTS_PY_REGISTER_OBJECT("register_medium", Medium)
}
