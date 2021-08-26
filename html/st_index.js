(function(){
  $("#statebutton").click(function(){
    var snl = $("table.snl");
    if (snl.hasClass("mhid")) {
      snl.removeClass("mhid");
    } else {
      snl.addClass("mhid");
    }
  });
})();
