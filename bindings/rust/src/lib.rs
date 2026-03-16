#[cxx::bridge(namespace = "qbuem::rust")]
mod ffi {
    struct TapeNode {
        meta: u32,
        offset: u32,
    }

    unsafe extern "C++" {
        include!("qbuem_rust_shim.hpp");

        type RustDocument;

        fn create_doc() -> UniquePtr<RustDocument>;
        fn parse(doc: Pin<&mut RustDocument>, json: &str) -> Result<()>;
        
        // Direct access to the internal Tape
        fn get_tape_ptr(doc: &RustDocument) -> *const TapeNode;
        fn get_tape_size(doc: &RustDocument) -> usize;
        fn get_source_ptr(doc: &RustDocument) -> *const u8;
        fn get_source_size(doc: &RustDocument) -> usize;

        // Complex ops
        fn dump(doc: &RustDocument, idx: u32, indent: i32) -> String;
        fn dump_to(doc: &RustDocument, idx: u32, indent: i32, out: &mut String);

        // Mutations
        fn set_null(doc: Pin<&mut RustDocument>, idx: u32);
        fn set_bool(doc: Pin<&mut RustDocument>, idx: u32, b: bool);
        fn set_int(doc: Pin<&mut RustDocument>, idx: u32, i: i64);
        fn set_double(doc: Pin<&mut RustDocument>, idx: u32, d: f64);
        fn set_string(doc: Pin<&mut RustDocument>, idx: u32, s: &str);
        fn insert_raw(doc: Pin<&mut RustDocument>, idx: u32, key: &str, raw_json: &str);
        fn erase_key(doc: Pin<&mut RustDocument>, idx: u32, key: &str);
        fn erase_idx(doc: Pin<&mut RustDocument>, idx: u32, idx_to_erase: usize);
    }
}

use std::slice;
use std::str;

pub struct Document {
    inner: cxx::UniquePtr<ffi::RustDocument>,
    tape: &'static [ffi::TapeNode],
    source: &'static [u8],
}

impl Document {
    pub fn new() -> Self {
        Document { inner: ffi::create_doc(), tape: &[], source: &[] }
    }

    pub fn parse(&mut self, json: &str) -> Result<(), cxx::Exception> {
        ffi::parse(self.inner.pin_mut(), json)?;
        unsafe {
            let t_ptr = ffi::get_tape_ptr(&self.inner);
            let t_size = ffi::get_tape_size(&self.inner);
            let s_ptr = ffi::get_source_ptr(&self.inner);
            let s_size = ffi::get_source_size(&self.inner);
            self.tape = slice::from_raw_parts(t_ptr, t_size);
            self.source = slice::from_raw_parts(s_ptr, s_size);
        }
        Ok(())
    }

    pub fn root(&self) -> Value<'_> { Value { doc: self, idx: 0 } }
}

#[derive(Clone, Copy)]
pub struct Value<'a> {
    doc: &'a Document,
    idx: u32,
}

#[repr(u8)]
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum Type {
    Null = 0, BooleanTrue, BooleanFalse, Integer, Double, StringRaw,
    NumberRaw, ArrayStart, ArrayEnd, ObjectStart, ObjectEnd,
}

#[inline(always)]
fn hash_key(s: &str) -> u8 {
    let b = s.as_bytes();
    if b.is_empty() { return 0; }
    (b[0] ^ b[b.len() - 1] ^ (b.len() as u8)) & 0x3F
}

impl<'a> Value<'a> {
    #[inline(always)]
    fn node(&self) -> &ffi::TapeNode { &self.doc.tape[self.idx as usize] }

    #[inline(always)]
    pub fn kind(&self) -> Type {
        let t = (self.node().meta >> 24) as u8;
        unsafe { std::mem::transmute(t) }
    }

    pub fn is_valid(&self) -> bool { self.idx < self.doc.tape.len() as u32 }
    pub fn is_null(&self) -> bool { self.kind() == Type::Null }
    pub fn is_bool(&self) -> bool { matches!(self.kind(), Type::BooleanTrue | Type::BooleanFalse) }
    pub fn is_int(&self) -> bool { self.kind() == Type::Integer }
    pub fn is_double(&self) -> bool { matches!(self.kind(), Type::Double | Type::NumberRaw) }
    pub fn is_string(&self) -> bool { self.kind() == Type::StringRaw }
    pub fn is_array(&self) -> bool { self.kind() == Type::ArrayStart }
    pub fn is_object(&self) -> bool { self.kind() == Type::ObjectStart }

    pub fn size(&self) -> usize {
        match self.kind() {
            Type::ArrayStart | Type::ObjectStart => (self.node().meta >> 16) as u8 as usize,
            _ => 0,
        }
    }

    pub fn get(&self, key: &str) -> Value<'a> {
        if self.kind() != Type::ObjectStart { return self.invalid(); }
        let target_hash = hash_key(key) as u32;
        let mut i = self.idx + 1;
        let end = self.doc.tape.len() as u32;
        while i < end {
            let node = &self.doc.tape[i as usize];
            let meta = node.meta;
            let t = (meta >> 24) as u8;
            if t == Type::ObjectEnd as u8 { break; }
            if t == Type::StringRaw as u8 {
                let fingerprint = (meta >> 18) & 0x3F;
                let len = meta as u16 as usize;
                if fingerprint == target_hash && len == key.len() {
                    let offset = node.offset as usize;
                    let k = unsafe { str::from_utf8_unchecked(&self.doc.source[offset..offset+len]) };
                    if k == key {
                        return Value { doc: self.doc, idx: i + 1 };
                    }
                }
                i = self.skip_value(i + 1);
            } else { i += 1; }
        }
        self.invalid()
    }

    pub fn index(&self, idx: usize) -> Value<'a> {
        if self.kind() != Type::ArrayStart { return self.invalid(); }
        let mut i = self.idx + 1;
        let mut count = 0;
        let end = self.doc.tape.len() as u32;
        while i < end {
            let t = (self.doc.tape[i as usize].meta >> 24) as u8;
            if t == Type::ArrayEnd as u8 { break; }
            if count == idx { return Value { doc: self.doc, idx: i }; }
            i = self.skip_value(i);
            count += 1;
        }
        self.invalid()
    }

    pub fn at(&self, path: &str) -> Value<'a> {
        if !path.starts_with('/') {
            return if path.is_empty() { *self } else { self.invalid() };
        }
        let mut current = *self;
        for part in path.split('/').skip(1) {
            if part.is_empty() { continue; }
            let part = part.replace("~1", "/").replace("~0", "~");
            if current.is_object() {
                current = current.get(&part);
            } else if current.is_array() {
                if let Ok(idx) = part.parse::<usize>() {
                    current = current.index(idx);
                } else { return self.invalid(); }
            } else { return self.invalid(); }
            if !current.is_valid() { return current; }
        }
        current
    }

    pub fn items(&self) -> ObjectItems<'a> {
        ObjectItems { doc: self.doc, idx: self.idx + 1, end: self.doc.tape.len() as u32 }
    }

    pub fn elements(&self) -> ArrayElements<'a> {
        ArrayElements { doc: self.doc, idx: self.idx + 1, end: self.doc.tape.len() as u32 }
    }

    fn invalid(&self) -> Value<'a> { Value { doc: self.doc, idx: u32::MAX } }

    fn skip_value(&self, idx: u32) -> u32 {
        let node = &self.doc.tape[idx as usize];
        let t = (node.meta >> 24) as u8;
        if t == Type::ObjectStart as u8 || t == Type::ArrayStart as u8 {
            let dist = (node.meta & 0xFFFF) as u32;
            if dist < 0xFFFF { return idx + dist + 1; }
            let mut i = idx + 1;
            let mut depth = 1;
            while depth > 0 && i < self.doc.tape.len() as u32 {
                let nt = (self.doc.tape[i as usize].meta >> 24) as u8;
                if nt == Type::ObjectStart as u8 || nt == Type::ArrayStart as u8 { depth += 1; }
                else if nt == Type::ObjectEnd as u8 || nt == Type::ArrayEnd as u8 { depth -= 1; }
                i += 1;
            }
            i
        } else { idx + 1 }
    }

    pub fn as_bool(&self) -> bool { self.kind() == Type::BooleanTrue }
    pub fn as_i64(&self) -> i64 {
        if !self.is_int() { return 0; }
        let offset = self.node().offset as usize;
        let len = self.node().meta as u16 as usize;
        unsafe { str::from_utf8_unchecked(&self.doc.source[offset..offset+len]) }.parse().unwrap_or(0)
    }
    pub fn as_f64(&self) -> f64 {
        if !self.is_double() && !self.is_int() { return 0.0; }
        let offset = self.node().offset as usize;
        let len = self.node().meta as u16 as usize;
        unsafe { str::from_utf8_unchecked(&self.doc.source[offset..offset+len]) }.parse().unwrap_or(0.0)
    }
    pub fn as_str(&self) -> &'a str {
        if !self.is_string() { return ""; }
        let offset = self.node().offset as usize;
        let len = self.node().meta as u16 as usize;
        unsafe { str::from_utf8_unchecked(&self.doc.source[offset..offset+len]) }
    }

    pub fn dump(&self, indent: i32) -> String { ffi::dump(&self.doc.inner, self.idx, indent) }
    pub fn dump_to(&self, out: &mut String, indent: i32) { ffi::dump_to(&self.doc.inner, self.idx, indent, out); }
    pub fn to_string(&self) -> String {
        use std::cell::RefCell;
        thread_local! { static BUFFER: RefCell<String> = RefCell::new(String::with_capacity(1024)); }
        BUFFER.with(|buf| { let mut b = buf.borrow_mut(); self.dump_to(&mut b, 0); b.clone() })
    }

    pub fn set_null(&self) { unsafe { ffi::set_null(self.doc.inner_mut(), self.idx) } }
    pub fn set_bool(&self, b: bool) { unsafe { ffi::set_bool(self.doc.inner_mut(), self.idx, b) } }
    pub fn set_int(&self, i: i64) { unsafe { ffi::set_int(self.doc.inner_mut(), self.idx, i) } }
    pub fn set_double(&self, d: f64) { unsafe { ffi::set_double(self.doc.inner_mut(), self.idx, d) } }
    pub fn set_string(&self, s: &str) { unsafe { ffi::set_string(self.doc.inner_mut(), self.idx, s) } }
}

impl Document {
    unsafe fn inner_mut(&self) -> std::pin::Pin<&mut ffi::RustDocument> {
        let ptr = &self.inner as *const cxx::UniquePtr<ffi::RustDocument> as *mut cxx::UniquePtr<ffi::RustDocument>;
        (*ptr).pin_mut()
    }
}

pub struct ObjectItems<'a> { doc: &'a Document, idx: u32, end: u32 }
impl<'a> Iterator for ObjectItems<'a> {
    type Item = (&'a str, Value<'a>);
    fn next(&mut self) -> Option<Self::Item> {
        if self.idx >= self.end { return None; }
        let node = &self.doc.tape[self.idx as usize];
        let t = (node.meta >> 24) as u8;
        if t == Type::ObjectEnd as u8 { return None; }
        if t == Type::StringRaw as u8 {
            let len = node.meta as u16 as usize;
            let offset = node.offset as usize;
            let key = unsafe { str::from_utf8_unchecked(&self.doc.source[offset..offset+len]) };
            self.idx += 1;
            let val = Value { doc: self.doc, idx: self.idx };
            self.idx = val.skip_value(self.idx);
            return Some((key, val));
        }
        None
    }
}

pub struct ArrayElements<'a> { doc: &'a Document, idx: u32, end: u32 }
impl<'a> Iterator for ArrayElements<'a> {
    type Item = Value<'a>;
    fn next(&mut self) -> Option<Self::Item> {
        if self.idx >= self.end { return None; }
        let t = (self.doc.tape[self.idx as usize].meta >> 24) as u8;
        if t == Type::ArrayEnd as u8 { return None; }
        let val = Value { doc: self.doc, idx: self.idx };
        self.idx = val.skip_value(self.idx);
        Some(val)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_basic_parsing() {
        let mut doc = Document::new();
        doc.parse(r#"{"name": "Alice", "age": 30, "scores": [1, 2, 3]}"#).unwrap();
        let root = doc.root();
        assert_eq!(root.get("name").as_str(), "Alice");
        assert_eq!(root.get("age").as_i64(), 30);
        assert_eq!(root.get("scores").index(2).as_i64(), 3);
    }
    #[test]
    fn test_at_pointer() {
        let mut doc = Document::new();
        doc.parse(r#"{"a": {"b": [10, 20]}}"#).unwrap();
        assert_eq!(doc.root().at("/a/b/1").as_i64(), 20);
    }
}
