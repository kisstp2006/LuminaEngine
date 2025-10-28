#pragma once
#include <stack>
#include <variant>

#include "Containers/Array.h"
#include "Core/Variant/Variant.h"

namespace Lumina
{
    template<typename T>
    concept TransactionType = requires(T t)
    {
        { t.Undo() } -> std::same_as<void>;
        { t.Redo() } -> std::same_as<void>;
    };
    
    template<TransactionType... Transactions>
    class TTransactionManager
    {
    public:
        using CommandVariant = std::variant<Transactions...>;
        
        class FScope
        {
        public:
            
            template<typename TCmd, typename... Args>
            FScope(TTransactionManager& InManager, std::in_place_type_t<TCmd>, Args&&... args)
                : Manager(InManager)
                , Command(std::in_place_type<TCmd>, std::forward<Args>(args)...)
            {
            }
            
            ~FScope()
            {
                if (!Committed)
                {
                    Manager.Execute(std::move(Command));
                }
            }
            
            FScope(const FScope&) = delete;
            FScope& operator=(const FScope&) = delete;
            FScope(FScope&& other) noexcept 
                : Manager(other.Manager)
                , Command(std::move(other.Command))
                , Committed(other.Committed)
            {
                other.Committed = true;
            }
            
            void Cancel() { Committed = true; }
            
        private:
            TTransactionManager& Manager;
            CommandVariant Command;
            bool Committed = false;
        };

        /** Starts a transaction scope, when it goes out of scope, it will be executed */
        template<typename TCmd, typename... Args>
        FScope BeginTransaction(Args&&... args)
        {
            return FScope(*this, std::in_place_type_t<TCmd>(), std::forward<Args>(args)...);
        }
        
        
        void Execute(CommandVariant&& command)
        {
            std::visit([](auto& cmd) { cmd.Redo(); }, command);
            UndoStack.push(std::move(command));
            RedoStack = {};
        }
        
        bool CanUndo() const { return !UndoStack.empty(); }
        bool CanRedo() const { return !RedoStack.empty(); }
        
        void Undo()
        {
            if (!CanUndo())
            {
                return;
            }

            auto command = UndoStack.top();
            UndoStack.pop();
            std::visit([](auto& cmd) { cmd.Undo(); }, command);
            RedoStack.push(std::move(command));
        }
        
        void Redo()
        {
            if (!CanRedo())
            {
                return;
            }

            auto command = RedoStack.top();
            RedoStack.pop();
            std::visit([](auto& cmd) { cmd.Redo(); }, command);
            UndoStack.push(std::move(command));
        }
        
        void Clear()
        {
            UndoStack = std::stack<CommandVariant>{};
            RedoStack = std::stack<CommandVariant>{};
        }
        
        size_t GetUndoCount() const { return UndoStack.Num(); }
        size_t GetRedoCount() const { return RedoStack.Num(); }
    
    private:
        std::stack<CommandVariant> UndoStack;
        std::stack<CommandVariant> RedoStack;
    };
    
    
}

