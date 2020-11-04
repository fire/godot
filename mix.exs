defmodule Godot.Mixfile do
  use Mix.Project

  def project do
    [
      app: :godot,
      compilers: [:unifex, :bundlex] ++ Mix.compilers,
      version: "0.1.0",
      deps: deps(),
      extra_applications: [:logger]
   ]
  end

  def application do
    [
      extra_applications: [:logger]
    ]
  end

  defp deps() do
    [
      {:unifex, git: "https://github.com/membraneframework/unifex.git", tag: "02c0564781ea5300486f96c9b9f9ce851d9211d7"},
    ]
  end
end
