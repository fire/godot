module Godot

interface [CNode]

state_type "MyState"

spec init(state, arguments :: [string]) :: {:ok :: label, state, code :: int}
                                  | {:fail :: label, state, result :: string}
spec iteration(state, delta :: float) :: {:ok :: label, state, code :: int}
                                  | {:fail :: label, state, result :: string}
spec call(state, method :: string)  :: {:ok :: label, state, :string :: label, result_string :: string}
                                  # | {:ok :: label, state, :float :: label, result_string :: float}
                                  | {:ok :: label, state, :int :: label, result_string :: int}
                                  | {:ok :: label, state, :bool ::label, result_string :: bool}
                                  | {:fail :: label, state, result :: string}
