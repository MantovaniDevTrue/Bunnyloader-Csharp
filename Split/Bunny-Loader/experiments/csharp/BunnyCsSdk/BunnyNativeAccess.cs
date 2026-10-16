using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace BunnyLoader.Source;

public enum BunnyAccessStatus { Ok,Unsupported=-1,Unavailable=-2,NotFound=-3,Ambiguous=-4,TypeMismatch=-5,ReadOnly=-6,Expired=-7,Busy=-8,TimedOut=-9,NativeException=-10,Capacity=-11,WrongThread=-12,Invalid=-13 }

public sealed class BunnyAccessException : InvalidOperationException
{
    public BunnyAccessStatus Status { get; }
    public BunnyAccessException(BunnyAccessStatus status,string operation) : base(operation+": "+status) { Status=status; }
}

public readonly struct BunnyNativeObject
{
    internal readonly int Slot;
    internal readonly long Ticket;
    internal readonly nint Self;
    internal BunnyNativeObject(int slot,long ticket,nint self) { Slot=slot;Ticket=ticket;Self=self; }
    public bool IsCurrent => Ticket!=0&&BunnyHooks.IsCurrent(Slot,Ticket);
    public static BunnyNativeObject FromHook(nint self)
    {
        BunnyHooks.GetNativeContext(out int slot,out long ticket,out nint actual);
        if(ticket==0||self!=actual||self==0)throw new BunnyAccessException(BunnyAccessStatus.Expired,"Objeto do hook");
        return new(slot,ticket,self);
    }
}

public sealed class BunnyNativeScope : IDisposable
{
    internal readonly long Token;
    private int disposed;
    internal BunnyNativeScope()
    {
        Token=NativeAccess.Open();
        if(Token<=0)throw new BunnyAccessException((BunnyAccessStatus)Token,"Abrir acesso nativo");
    }
    public static string GetStatus() => "Acesso C#: vinculos="+NativeAccess.Stat(0)+" leituras="+NativeAccess.Stat(1)+
        " escritas="+NativeAccess.Stat(2)+" metodos="+NativeAccess.Stat(3)+" lotes="+NativeAccess.Stat(4)+
        " falhas="+NativeAccess.Stat(5)+" esperasVencidas="+NativeAccess.Stat(6)+" ultimo="+NativeAccess.Stat(7)+" escopos="+NativeAccess.Stat(8)+" esperaTotalUs="+NativeAccess.Stat(9)+
        " jogoTotalUs="+NativeAccess.Stat(10)+" picoEsperaUs="+NativeAccess.Stat(11)+
        " ocupadoRpc="+NativeAccess.Stat(12);
    public bool IsDisposed => Volatile.Read(ref disposed)!=0;
    internal void RequireLive() { if(IsDisposed)throw new ObjectDisposedException(nameof(BunnyNativeScope)); }
    public BunnyNativeType Type(string fullName)
    {
        RequireLive();NativeAccess.Name(fullName,256);
        if(!fullName.StartsWith("Terraria.",StringComparison.Ordinal))throw new ArgumentException("Classe Terraria esperada",nameof(fullName));
        return new(this,NativeAccess.BindChecked(Token,0,0,BunnyValueKind.Void,0,fullName,""),fullName);
    }
    public BunnyNativeBatch Batch(params BunnyNativeMember[] members) => new(this,members);
    public void Dispose() { if(Interlocked.Exchange(ref disposed,1)==0)NativeAccess.Close(Token); }
}

public sealed class BunnyNativeType
{
    internal readonly BunnyNativeScope Scope;
    internal readonly int Id;
    public string Name { get; }
    internal BunnyNativeType(BunnyNativeScope scope,int id,string name) { Scope=scope;Id=id;Name=name; }
    public BunnyNativeMember Field(string name,BunnyValueKind kind,bool isStatic=false,bool writable=false) => Bind(name,kind,1,isStatic,writable,"");
    public BunnyNativeMember Property(string name,BunnyValueKind kind,bool isStatic=false,bool writable=false) => Bind(name,kind,2,isStatic,writable,"");
    public BunnyNativeMember Method(string name,BunnyValueKind result,bool isStatic=false,params string[] parameterTypes)
    {
        if(parameterTypes is null||parameterTypes.Length>4)throw new ArgumentException("Ate quatro parametros por metodo",nameof(parameterTypes));
        foreach(string type in parameterTypes)NativeAccess.Name(type,120);
        return Bind(name,result,3,isStatic,false,string.Join("|",parameterTypes),parameterTypes.Length);
    }
    private BunnyNativeMember Bind(string name,BunnyValueKind kind,int category,bool isStatic,bool writable,string parameters,int argc=0)
    {
        Scope.RequireLive();NativeAccess.Name(name,128);
        if(name[0]=='.')throw new NotSupportedException("Construtores nativos nao fazem parte deste lote");
        if((uint)kind>(uint)BunnyValueKind.UInt64||(category!=3&&kind==BunnyValueKind.Void))throw new ArgumentOutOfRangeException(nameof(kind));
        int id=NativeAccess.BindChecked(Scope.Token,Id,category,kind,(isStatic?1:0)|(writable?2:0),name,parameters);
        return new(Scope,id,kind,isStatic,writable,category,argc,Name+"."+name);
    }
}

public sealed class BunnyNativeMember
{
    internal readonly BunnyNativeScope Scope;
    internal readonly int Id,Category,ArgumentCount;
    public BunnyValueKind Kind { get; }
    public bool IsStatic { get; }
    public bool CanWrite { get; }
    public string Name { get; }
    internal BunnyNativeMember(BunnyNativeScope scope,int id,BunnyValueKind kind,bool isStatic,bool writable,int category,int argc,string name)
    { Scope=scope;Id=id;Kind=kind;IsStatic=isStatic;CanWrite=writable;Category=category;ArgumentCount=argc;Name=name; }
    internal void Context(BunnyNativeObject target,out int slot,out long ticket,out nint self)
    {
        Scope.RequireLive();BunnyHooks.GetNativeContext(out slot,out ticket,out self);
        if(IsStatic&&ticket==0) { slot=-1;self=0;return; }
        if(ticket==0||(!IsStatic&&(!target.IsCurrent||target.Ticket!=ticket||target.Self!=self)))
            throw new BunnyAccessException(BunnyAccessStatus.Expired,Name);
    }
    public BunnyValue Read(BunnyNativeObject target=default)
    {
        if(Category!=1&&Category!=2)throw new BunnyAccessException(BunnyAccessStatus.Invalid,Name);
        return Call(2,target,default,default,default,default,0);
    }
    public void Write(BunnyValue value,BunnyNativeObject target=default)
    {
        if(!CanWrite)throw new BunnyAccessException(BunnyAccessStatus.ReadOnly,Name);
        Call(3,target,value,default,default,default,1);
    }
    public BunnyValue Invoke(BunnyNativeObject target=default,BunnyValue arg0=default,BunnyValue arg1=default,BunnyValue arg2=default,BunnyValue arg3=default)
    {
        if(Category!=3)throw new BunnyAccessException(BunnyAccessStatus.Invalid,Name);
        return Call(4,target,arg0,arg1,arg2,arg3,ArgumentCount);
    }
    private BunnyValue Call(int op,BunnyNativeObject target,BunnyValue a0,BunnyValue a1,BunnyValue a2,BunnyValue a3,int count)
    {
        Context(target,out int slot,out long ticket,out nint self);
        var args=new NativeAccess.Arguments { A=a0,B=a1,C=a2,D=a3 };
        var status=NativeAccess.Call(Scope.Token,Id,op,slot,ticket,self,count,in args,out BunnyValue result);
        if(status!=BunnyAccessStatus.Ok)throw new BunnyAccessException(status,Name);
        return result;
    }
}

public sealed class BunnyNativeBatch
{
    private readonly BunnyNativeScope scope;
    private readonly BunnyNativeMember[] members;
    private readonly int[] ids;
    private readonly BunnyValue[] scratch;
    internal BunnyNativeBatch(BunnyNativeScope scope,BunnyNativeMember[] members)
    {
        scope.RequireLive();
        if(members is null||members.Length<1||members.Length>16)throw new ArgumentException("Lote de 1 a 16 leituras",nameof(members));
        this.scope=scope;this.members=(BunnyNativeMember[])members.Clone();ids=new int[members.Length];scratch=new BunnyValue[members.Length];
        for(int i=0;i<members.Length;++i) {
            var member=this.members[i];
            if(member is null||member.Scope!=scope||(member.Category!=1&&member.Category!=2))throw new ArgumentException("Membros de leitura do mesmo escopo",nameof(members));
            ids[i]=member.Id;
        }
    }
    public int Count => members.Length;
    public BunnyValue ReadAndInvoke(BunnyNativeMember method,BunnyValue[] destination,
        BunnyNativeObject target=default,BunnyValue arg0=default,BunnyValue arg1=default,
        BunnyValue arg2=default,BunnyValue arg3=default)
    {
        scope.RequireLive();
        if(method is null||method.Scope!=scope||method.Category!=3)
            throw new ArgumentException("Metodo nativo do mesmo escopo",nameof(method));
        if(destination is null||destination.Length<Count)
            throw new ArgumentException("Destino insuficiente",nameof(destination));
        int slot=0;long ticket=0;nint self=0;
        foreach(var member in members)member.Context(target,out slot,out ticket,out self);
        method.Context(target,out slot,out ticket,out self);
        var args=new NativeAccess.Arguments { A=arg0,B=arg1,C=arg2,D=arg3 };
        var status=NativeAccess.ReadBatchInvoke(scope.Token,slot,ticket,self,ids,scratch,Count,
            method.Id,method.ArgumentCount,in args,out BunnyValue result);
        if(status!=BunnyAccessStatus.Ok)
            throw new BunnyAccessException(status,"Lote e metodo nativos");
        Array.Copy(scratch,destination,Count);
        return result;
    }
    public void Read(BunnyValue[] destination,BunnyNativeObject target=default)
    {
        scope.RequireLive();
        if(destination is null||destination.Length<Count)throw new ArgumentException("Destino insuficiente",nameof(destination));
        int slot=0;long ticket=0;nint self=0;
        foreach(var member in members)member.Context(target,out slot,out ticket,out self);
        var status=NativeAccess.ReadBatch(scope.Token,slot,ticket,self,ids,scratch,Count);
        if(status!=BunnyAccessStatus.Ok)throw new BunnyAccessException(status,"Leitura nativa em lote");
        Array.Copy(scratch,destination,Count);
    }
}

internal static class NativeAccess
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct Arguments { public BunnyValue A,B,C,D; }
    internal static void Name(string name,int max)
    {
        if(string.IsNullOrWhiteSpace(name)||name.Length>max||name.IndexOf('\0')>=0||name.Contains('|'))throw new ArgumentException("Nome nativo invalido",nameof(name));
    }
    internal static int BindChecked(long scope,int parent,int category,BunnyValueKind kind,int flags,string name,string parameters)
    {
        int id=Bind(scope,parent,category,kind,flags,name,parameters);
        if(id<=0)throw new BunnyAccessException((BunnyAccessStatus)id,"Vincular "+name);
        return id;
    }
    [DllImport("bunny",EntryPoint="bunny_native_access_stat",CallingConvention=CallingConvention.Cdecl)]
    internal static extern long Stat(int key);
    [DllImport("bunny",EntryPoint="bunny_native_open",CallingConvention=CallingConvention.Cdecl)]
    internal static extern long Open();
    [DllImport("bunny",EntryPoint="bunny_native_close",CallingConvention=CallingConvention.Cdecl)]
    internal static extern void Close(long token);
    [DllImport("bunny",EntryPoint="bunny_native_bind",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Bind(long scope,int parent,int category,BunnyValueKind kind,int flags,[MarshalAs(UnmanagedType.LPUTF8Str)]string name,[MarshalAs(UnmanagedType.LPUTF8Str)]string parameters);
    [DllImport("bunny",EntryPoint="bunny_native_call",CallingConvention=CallingConvention.Cdecl)]
    internal static extern BunnyAccessStatus Call(long scope,int member,int op,int slot,long ticket,nint self,int count,in Arguments args,out BunnyValue result);
    [DllImport("bunny",EntryPoint="bunny_native_read_batch",CallingConvention=CallingConvention.Cdecl)]
    internal static extern BunnyAccessStatus ReadBatch(long scope,int slot,long ticket,nint self,[In]int[] members,[Out]BunnyValue[] result,int count);
    [DllImport("bunny",EntryPoint="bunny_native_read_batch_invoke",CallingConvention=CallingConvention.Cdecl)]
    internal static extern BunnyAccessStatus ReadBatchInvoke(long scope,int slot,long ticket,nint self,
        [In]int[] members,[Out]BunnyValue[] result,int count,int method,int argCount,
        in Arguments args,out BunnyValue methodResult);
}
