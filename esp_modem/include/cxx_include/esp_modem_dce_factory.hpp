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

    template <typename T, typename Ptr, typename ...Args>
    static auto make(Args&&... args) -> typename std::enable_if<std::is_same<Ptr, T*>::value, T*>::type
    {
        return new T(std::forward<Args>(args)...);
    }

    template <typename T, typename Ptr, typename ...Args>
    static auto make(Args&&... args) ->  typename std::enable_if<std::is_same<Ptr, std::shared_ptr<T>>::value, std::shared_ptr<T>>::type
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename Ptr = std::unique_ptr<T>, typename ...Args>
    static auto make(Args&&... args) -> typename std::enable_if<std::is_same<Ptr, std::unique_ptr<T>>::value, std::unique_ptr<T>>::type
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

};

template<typename Module>
class Builder {
    static_assert(std::is_base_of<ModuleIf, Module>::value, "Builder must be used only for Module classes");
public:
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
    auto create_module(const esp_modem_dce_config *config) -> Ptr
    {
        return FactoryHelper::make<Module, Ptr>(dte, config);
    }

    template<typename DceT, typename Ptr>
    auto create(const esp_modem_dce_config *config) -> Ptr
    {
        if (dte == nullptr)
            return nullptr;
        if (module == nullptr) {
            module = create_module<decltype(module)>(config);
            if (module == nullptr)
                return nullptr;
        }
        return FactoryHelper::make<DceT, Ptr>(std::move(dte), std::move(module), netif);
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

    template <typename Module, typename ...Args>
    static std::unique_ptr<DCE> build_unique(const config *cfg, Args&&... args)
    {
        return build_generic_DCE<Module, DCE, std::unique_ptr<DCE>>(cfg, std::forward<Args>(args)...);
    }

    template <typename Module, typename ...Args>
    static DCE* build(const config *cfg, Args&&... args)
    {
        return build_generic_DCE<Module, DCE>(cfg, std::forward<Args>(args)...);
    }


    template <typename Module, typename ...Args>
    std::shared_ptr<Module> build_shared_module(const config *cfg, Args&&... args)
    {
        return build_module_T<Module>(cfg, std::forward<Args>(args)...);
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

    template <typename Module, typename Ptr = std::shared_ptr<Module>, typename ...Args>
    Module build_module_T(const config *cfg, Args&&... args)
    {
        Builder<Module> b(std::forward<Args>(args)...);
        return b.template create_module<Module>(cfg);
    }

protected:
    template <typename Module, typename Dce = DCE_T<Module>, typename DcePtr = Dce*,  typename ...Args>
    static DcePtr build_generic_DCE(const config *cfg, Args&&... args)
    {
        Builder<Module> b(std::forward<Args>(args)...);
        return b.template create<Dce, DcePtr>(cfg);
    }
};

} // namespace esp_modem::dce_factory

#endif // _ESP_MODEM_DCE_FACTORY_HPP_
