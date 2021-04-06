// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _ESP_MODEM_DCE_FACTORY_HPP_
#define _ESP_MODEM_DCE_FACTORY_HPP_

namespace esp_modem::dce_factory {

using config = ::esp_modem_dce_config;

class FactoryHelper {
public:
    static std::unique_ptr<PdpContext> create_pdp_context(std::string &apn);

    template <typename T, typename ...Args>
    static auto make(T* &t, Args&&... args) -> std::remove_reference_t<decltype(t)>
    {
        t = new T(std::forward<Args>(args)...);
        return true;
    }

    template <typename T, typename ...Args>
    static bool make(std::shared_ptr<T> &t, Args&&... args)
    {
        t = std::make_shared<T>(std::forward<Args>(args)...);
        return true;
    }

    template <typename T, typename ...Args>
    static bool make(std::unique_ptr<T> &t, Args&&... args)
    {
        t = std::unique_ptr<T>(new T(std::forward<Args>(args)...));
        return true;
    }

};

template<typename Module>
class Builder {
    static_assert(std::is_base_of<ModuleIf, Module>::value, "Builder must be used only for Module classes");
public:
    explicit Builder(std::shared_ptr<DTE> dte): dte(std::move(dte))
    {
        esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_PPP();
        netif = esp_netif_new(&netif_config);
        throw_if_false(netif != nullptr, "Cannot create default PPP netif");
    }

    Builder(std::shared_ptr<DTE> x, esp_netif_t* esp_netif): dte(std::move(x)), module(nullptr), netif(esp_netif)
    {
        throw_if_false(netif != nullptr, "Null netif");
    }

    Builder(std::shared_ptr<DTE> dte, esp_netif_t* esp_netif, std::shared_ptr<Module> dev): dte(std::move(dte)), module(std::move(dev)), netif(esp_netif)
    {
        throw_if_false(netif != nullptr, "Null netif");
    }

    ~Builder()
    {
        throw_if_false(module == nullptr, "module was captured or created but never used");
    }


    template<typename Ptr>
    bool create_module(Ptr &t, const esp_modem_dce_config *config)
    {
        return FactoryHelper::make<Module>(t, dte, config);
    }

    template<typename DceT, typename Ptr>
    bool create(Ptr &t, const esp_modem_dce_config *config)
    {
        if (dte == nullptr)
            return false;
        if (module == nullptr) {
            if (!create_module(module, config)) {
                return false;
            }
        }
        return FactoryHelper::make<DceT>(t, std::move(dte), std::move(module), netif);
    }

private:
    std::shared_ptr<DTE> dte;
    std::shared_ptr<Module> module;
    esp_netif_t *netif;
};

enum class Modem {
    SIM800,
    SIM7600,
    BG96,
    MinModule
};

class Factory {
public:
    explicit Factory(Modem modem): m(modem) {}

    template <typename T, typename ...Args>
    static std::unique_ptr<DCE> build_unique(const config *cfg, Args&&... args)
    {
        return build_generic_DCE<DCE, std::unique_ptr<DCE>, T>(cfg, std::forward<Args>(args)...);
    }

    template <typename T, typename ...Args>
    static DCE* build(const config *cfg, Args&&... args)
    {
        return build_generic_DCE<DCE, DCE*, T>(cfg, std::forward<Args>(args)...);
    }


    template <typename T, typename ...Args>
    static std::shared_ptr<T> build_shared_module(const config *cfg, Args&&... args)
    {
        return build_module_T<std::shared_ptr<T>, T>(cfg, std::forward<Args>(args)...);
    }


    template <typename ...Args>
    std::shared_ptr<GenericModule> build_shared_module(const config *cfg, Args&&... args)
    {
        switch (m) {
            case Modem::SIM800:
                return build_shared_module<SIM800>(cfg, std::forward<Args>(args)...);
            case Modem::SIM7600:
                return build_shared_module<SIM7600>(cfg, std::forward<Args>(args)...);
            case Modem::BG96:
                return build_shared_module<BG96>(cfg, std::forward<Args>(args)...);
            case Modem::MinModule:
                break;
        }
        return nullptr;
    }

    template <typename ...Args>
    std::unique_ptr<DCE> build_unique(const config *cfg, Args&&... args)
    {
        switch (m) {
            case Modem::SIM800:
                return build_unique<SIM800>(cfg, std::forward<Args>(args)...);
            case Modem::SIM7600:
                return build_unique<SIM7600>(cfg, std::forward<Args>(args)...);
            case Modem::BG96:
                return build_unique<BG96>(cfg, std::forward<Args>(args)...);
            case Modem::MinModule:
                break;
        }
        return nullptr;
    }


private:
    Modem m;

    template <typename T, typename U, typename ...Args>
    static bool build_module_T(U &t, const config *cfg, Args&&... args)
    {
        Builder<T> b(std::forward<Args>(args)...);
        return b.create_module(t, cfg);
    }

    template <typename T, typename U, typename ...Args>
    static T build_module_T(const config *cfg, Args&&... args)
    {
        T module;
        if (build_module_T<U>(module, cfg, std::forward<Args>(args)...))
            return module;
        return nullptr;
    }

    template <typename Dce, typename DcePtr, typename Module, typename ...Args>
    static bool build_generic_DCE(DcePtr &t, const config *cfg, Args&&... args)
    {
        Builder<Module> b(std::forward<Args>(args)...);
        return b.template create<Dce>(t, cfg);
    }

protected:
    template <typename Dce, typename DcePtr, typename Module, typename ...Args>
    static DcePtr build_generic_DCE(const config *cfg, Args&&... args)
    {
        DcePtr dce;
        if (build_generic_DCE<Dce, DcePtr, Module>(dce, cfg, std::forward<Args>(args)...))
            return dce;
        return nullptr;
    }

};

} // namespace esp_modem::dce_factory

#endif // _ESP_MODEM_DCE_FACTORY_HPP_
