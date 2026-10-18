using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using Microsoft.CodeAnalysis;

namespace BunnyLoader.SourceCompiler;

/// <summary>Relatorio limitado, deterministico e legivel no editor do Android.</summary>
public static class SourceDiagnostics
{
    public static string Format(IEnumerable<Diagnostic> diagnostics, int maximum = 60)
    {
        if (maximum < 1 || maximum > 100) throw new ArgumentOutOfRangeException(nameof(maximum));
        var errors = diagnostics.Where(d => d.Severity == DiagnosticSeverity.Error)
            .OrderBy(d => d.Location.SourceTree?.FilePath ?? "", StringComparer.Ordinal)
            .ThenBy(d => d.Location.SourceSpan.Start).ThenBy(d => d.Id, StringComparer.Ordinal).ToArray();
        var output = new StringBuilder();
        output.AppendLine("BUNNY_COMPILE_ERRORS=" + errors.Length);
        foreach (var error in errors.Take(maximum))
        {
            var location = error.Location;
            var tree = location.SourceTree;
            var span = location.GetLineSpan(); // arquivo real, sem remapeamento de #line
            string path = tree is null ? "(sem arquivo)" : "content/" + tree.FilePath.Replace('\\', '/');
            output.Append(path).Append(':').Append(span.StartLinePosition.Line + 1)
                .Append(':').Append(span.StartLinePosition.Character + 1).Append(" [")
                .Append(error.Id).Append("] ").AppendLine(error.GetMessage(CultureInfo.InvariantCulture));
            if (tree is not null)
            {
                var line = tree.GetText().Lines[span.StartLinePosition.Line].ToString();
                // Janela em torno do erro: o marcador continua correto em linhas longas.
                int column = span.StartLinePosition.Character;
                int start = Math.Max(0, column - 80);
                string prefix = line.Substring(start, Math.Min(column, line.Length) - start).Replace("\t", "    ");
                string excerpt = line.Substring(start, Math.Min(200, line.Length - start)).Replace("\t", "    ");
                output.Append("  | ").AppendLine(excerpt);
                output.Append("  | ").Append(' ', prefix.Length).AppendLine("^");
            }
            string hint = Hint(error.Id);
            if (hint.Length != 0) output.Append("  Como corrigir: ").AppendLine(hint);
        }
        if (errors.Length > maximum)
            output.AppendLine("BUNNY_COMPILE_OMITTED=" + (errors.Length - maximum) + " (corrija os primeiros e compile novamente)");
        return output.ToString().TrimEnd();
    }

    private static string Hint(string id) => id switch
    {
        "CS0115" => "Confira nome, retorno e parametros do override no SDK Bunny; assinaturas do tModLoader nao sao equivalentes.",
        "CS1061" or "CS0117" => "Confira se o membro existe nesta versao do SDK e o tipo do objeto usado.",
        "CS0246" or "CS0103" => "Confira o nome, namespace/using e se o arquivo que declara esse tipo esta dentro de content/.",
        "CS1503" => "Confira o tipo de cada argumento; nao converta um indice de entidade em objeto do jogo.",
        "CS0534" => "Implemente os membros abstratos exigidos pela classe base; copie a assinatura do SDK.",
        "CS0101" or "CS0111" => "Ha uma declaracao duplicada. Renomeie a classe/metodo ou remova a copia dentro de content/.",
        "CS1002" => "Confira o ponto e virgula e a linha imediatamente anterior.",
        _ => ""
    };
}
