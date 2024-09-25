const DAY_PREF = 'd';
const ACT_PREF = 'a'

const LIST_DAY = ['Monday','Thusday','Wednesday','Thursday','Friday','Saturday','Sunday'];
// [formName [type, max limit, min limit,[inputNames],]]
const FORMS_LIST = [
['Network',[['text','32','1',['SSID']],['text','32','8',['PWD']]]],
['Openweather',[['text','32','1',['City']],['text','32','32',['Key']]]],
['Offset',[['number','23','0',['Hour']]]],
['Loud',[['number','99','0',['%']]]],
['Status',[['checkbox',1,,['Notifications','Openweather Ok','SNTP Ok',"STA conf. Ok"]]]]
];

const modal = window.document.getElementById('modal');


function addAction(day, td, str_val)
{
let rmBut = null;
if(td.children.length == 1){
rmBut = document.createElement('input');
rmBut.type = 'button';
rmBut.value = '-';
rmBut.onclick = ()=>{removeAction(td)};
rmBut.classList = 'controlBut danger';
td.insertBefore(rmBut, td.children[0]);
} else {
rmBut = td.children[td.children.length-2];
}
const position = td.children.length-2;
let inpTime = document.createElement('input');
inpTime.type = 'time';
inpTime.name =day+'t'+position;
inpTime.required = true;
if(str_val)inpTime.value = str_val;
td.insertBefore(inpTime, rmBut);
}

function removeAction(td)
{
td.removeChild(td.children[td.children.length-3]);
if(td.children.length == 2)
td.removeChild(td.children[td.children.length-2]);
}

function createNotificationForm(name)
{
const form = document.createElement('form');
form.name = name;
form.action = '';
const fieldset = document.createElement('fieldset');
const legend = document.createElement('legend');
legend.innerText = name;
fieldset.appendChild(form);
fieldset.appendChild(legend);
const tr_head = document.createElement('tr');
const table = document.createElement('table');
LIST_DAY.forEach((e)=>{
const th = document.createElement('th');
th.innerText = e;
tr_head.appendChild(th);
});
const tr_body = document.createElement('tbody');
const row = document.createElement('tr');
for(let day=0; day<LIST_DAY.length; ++day){
const td = document.createElement('td');
const input = document.createElement('input');
input.type = 'button';
input.value = '+';
td.id = DAY_PREF +day;
input.onclick = ()=>{addAction(day, td)};
input.classList.add('controlBut');
td.appendChild(input);
row.appendChild(td);
}
tr_body.appendChild(row);
table.appendChild(tr_head);
table.appendChild(tr_body);
const submit = document.createElement('input');
submit.value = 'Submit';
submit.type = 'submit';
form.appendChild(table);
form.appendChild(submit);
document.getElementById('settings_forms').appendChild(fieldset);
}

function setNotificationData(schema,notif_data="")
{
if(schema?.length==14 && notif_data?.length%4===0 && notif_data.length>0){
let vi=0, si=0;
for(let d=0; d<7; d++){
let td = document.getElementById(DAY_PREF+d);
if(td && td.children){
while(td.children.length>2){
removeAction(td);
}
const notif_num = +schema[si++]*10 + +schema[si++];
for(let n=0;n<notif_num;++n,vi+=4){
const time_min = +notif_data.slice(vi,vi+4);
const str_val = Math.trunc(time_min/60).toString().padStart(2,'0') + ":" + (time_min%60).toString().padStart(2,'0');
addAction(d, td, str_val);
}
}
}
}
}


function getSetting()
{
fetch('/data?', {
method:'POST',
mode: 'no-cors',
body: null,
})
.then((r) => r.json())
.then((r) => {
for(const key in r){
const value = r[key];
if(key === 'schema'){
setNotificationData(value, r['notif']);
} else if(key === 'Status'){
const flags = Number(value);
[...document.querySelectorAll('[type=checkbox]')].forEach((checkbox, i) =>{
checkbox.checked = flags&(1<<i);
});
} else if(key!='notif') {
const input = document.getElementById(key);
if(input)
input.value = value;
}
}
 })
.catch((e) => showModal(e));
}


function createForms()
{
const containerForms = document.getElementById('settings_forms');
FORMS_LIST.forEach((form_instance)=>{
const [formName, inputList] = form_instance;
const form = document.createElement('form');
const container = document.createElement('div');
const fieldset = document.createElement('fieldset');
const legend = document.createElement('legend');
legend.innerText = formName;
fieldset.appendChild(form);
fieldset.appendChild(legend);
form.name = formName;
form.action = '';
const submit = document.createElement('input');
submit.value = 'Submit';
submit.type = 'submit';
inputList.forEach((inputData)=>{
const [type, maxLimit, minLimit, inputNames] = inputData;
inputNames.forEach((inputName, i)=>{
const label = document.createElement('label');
label.innerText = inputName+' ';
const input = document.createElement('input');
input.type = type;
input.id = inputName;
input.name = inputName;
if(type === 'text'){
input.value = '';
input.maxLength = maxLimit;
input.minLength = minLimit;
input.placeholder = 'Enter '+ inputName;
} else if(type == 'checkbox' 
&& i >= maxLimit){
input.disabled = true;
} else if(type == 'number'){
input.max = maxLimit;
input.min = minLimit;
}
label.appendChild(input);
form.appendChild(label);
});
form.appendChild(submit);
});
container.appendChild(fieldset);
containerForms.appendChild(container);
});
}

function getInfo()
{
sendDataForm('info?');
}

function serverExit() 
{
sendDataForm('close');
document.getElementById('exit').classList.add('danger');
}


function showModal(str, success) 
{
modal.innerText = str ? str : ':(';
modal.style['background-color'] = success === true ? 'green':'red';
modal.classList.add('show');
const leftPos = (window.innerWidth - modal.offsetWidth) / 2;
const topPos = (window.innerHeight - modal.offsetHeight) / 2 + window.scrollY;
modal.style.right = leftPos + 'px';
modal.style.top = topPos + 'px';
setTimeout(() => {
modal.classList.remove('show');
},5000);
}



document.body.addEventListener('submit', (e) => {
e.preventDefault();
e.stopPropagation();
sendData(e.target.name);
});

const get_schema_str = (sch)=>sch.map(el=>Math.trunc(el/10)+''+ el%10).join('')

const get_min_str = (time="")=>(+time.slice(0,2)*60 + +time.slice(2,4)).toString().padStart(4,'0')


function sendData(formName)
{
const js = {};
const arr = [];
const schema = [0,0,0,0,0,0,0];
let data = null;
let i=0;
const childsList = document.forms[formName];
if(childsList){
for(const child of childsList){
let value = child.value;
if(value){
if(child.type === 'number'){
data = value;
} else if(child.type === 'text'){
if(data == null)
data = js;
js[child.name] = value;
} else if(child.type === 'time'){
const day = Number(child.name.split('t')[0]);
++schema[day];
const min_num = value.split(':').join('');
arr.push(get_min_str(min_num));
} else if(child.type === 'checkbox'){
if(data == null)
data = 0;
if(child.checked){
data |= 1<<i;
}
i++;
}
}
}
if(formName == 'Notification'){
data = JSON.stringify({
schema:get_schema_str(schema),
notif:arr.join('')
});
} else if(data === js){
data=JSON.stringify(js);
}
sendDataForm(formName, data);
}
}


async function sendDataForm(path, data=""){
let res = true;
await fetch('/'+ path, {
method:'POST',
mode:'no-cors',
body:data,
}).then((r)=>{
if(!r.ok)
res=false; 
return r.text()
}).then((r) => showModal(r, res))
.catch((e) => showModal(e, false));
}

function setTime() 
{
const data = "" + Math.trunc(new Date().getTime());
sendDataForm("time", data);
}

createForms();
createNotificationForm("Notification");

